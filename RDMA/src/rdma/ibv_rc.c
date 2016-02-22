#include <rdma_ibv_rc.h>

extern FILE *rdma_log_fp;

/* InfiniBand device */
extern ib_device_t *ib_device;
#define IBDEV ib_device
#define RDMA_DATA ((rdma_data_t*)ib_device->udata)

/**
 * Initialize RC data
 */
int rc_init()
{

    rc_prerequisite();
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "Cannot create RC prerequisite\n");
    }

    /* Register memory for RC */
    rc = rc_memory_reg();
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "Cannot register RC memory\n");
    }
    
    /* Create QPs for RC communication */
    for (i = 0; i < RDMA_DATA->config.len; i++) {
        ib_ep_t* ep = (ib_ep_t*)
                        RDMA_DATA->config.servers[i].ep;
        
        /* Create QPs for this endpoint */
        rc_qp_create(ep);
        if (0 != rc) {
            rdma_error_return(1, rdma_log_fp, "Cannot create QPs\n");
        }
    }
    
    return 0;
}

static int rc_prerequisite()
{
    /*
    if (IBDEV->rc_max_send_wr > IBDEV->ib_dev_attr.max_qp_wr) {
            IBDEV->rc_max_send_wr = IBDEV->ib_dev_attr.max_qp_wr;
    }*/

    IBDEV->rc_cqe = MAX_SERVER_COUNT * IBDEV->rc_max_send_wr;
    if (IBDEV->rc_cqe > IBDEV->ib_dev_attr.max_cqe) {
        IBDEV->rc_cqe = IBDEV->ib_dev_attr.max_cqe;
    }

    /* Allocate a RC protection domain */
    IBDEV->rc_pd = ibv_alloc_pd(IBDEV->ib_dev_context);
    if (NULL == IBDEV->rc_pd) {
        rdma_error_return(1, rdma_log_fp, "Cannot create PD\n");
    }

    /* Create a RC completion queue */
    IBDEV->rc_cq[LOG_QP] = ibv_create_cq(IBDEV->ib_dev_context, 
                                   IBDEV->rc_cqe, NULL, NULL, 0);
    if (NULL == IBDEV->rc_cq[LOG_QP]) {
        rdma_error_return(1, rdma_log_fp, "Cannot create LOG CQ\n");
    }

    if (0 != find_max_inline(IBDEV->ib_dev_context,
                            IBDEV->rc_pd,
                            &IBDEV->rc_max_inline_data))
    {
        rdma_error_return(1, rdma_log_fp, "Cannot find max RC inline data\n");
    }

    /* Allocate array for work completion */
    IBDEV->rc_wc_array = (struct ibv_wc*)
            malloc(IBDEV->rc_cqe * sizeof(struct ibv_wc));
    if (NULL == IBDEV->rc_wc_array) {
        rdma_error_return(1, rdma_log_fp, "Cannot allocate array for WC\n");
    }

    return 0;
}

int rc_memory_reg()
{
    /* Register memory for local log */  
    IBDEV->lcl_mr[LOG_QP] = ibv_reg_mr(IBDEV->rc_pd,
            RDMA_DATA->log, sizeof(log_t) + RDMA_DATA->log->len, 
            IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC | 
            IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE);
    if (NULL == IBDEV->lcl_mr[LOG_QP]) {
        rdma_error_return(1, rdma_log_fp, "Cannot register memory because %s\n", 
                    strrdma_error(errno));
    }

    return 0;
}

int rc_qp_create(ib_ep_t* ep)
{
    int i;
    struct ibv_qp_init_attr qp_init_attr;
    
    if (NULL == ep) return 0;

    /** qp_init_attr describes the requested attributes of the newly created QP
      * cap describes the size of the Queue Pair
      * max_recv_wr, the maximum number of outstanding Work Requests that can be posted to the receive Queue in that Queue Pair
      * max_inline_data, the maximum message size (in bytes) that can be posted inline to the Send Queue */
    for (i = 1; i < 2; i++) {
        /* Create RC QP */
        memset(&qp_init_attr, 0, sizeof(qp_init_attr));
        qp_init_attr.qp_type = IBV_QPT_RC;
        qp_init_attr.send_cq = IBDEV->rc_cq[i];
        qp_init_attr.recv_cq = IBDEV->rc_cq[i];
        qp_init_attr.cap.max_inline_data = IBDEV->rc_max_inline_data;
        qp_init_attr.cap.max_send_sge = 1;  
        qp_init_attr.cap.max_recv_sge = 1;
        qp_init_attr.cap.max_recv_wr = 1;
        qp_init_attr.cap.max_send_wr = IBDEV->rc_max_send_wr;
        ep->rc_ep.rc_qp[i].qp = ibv_create_qp(IBDEV->rc_pd, &qp_init_attr);
        if (NULL == ep->rc_ep.rc_qp[i].qp) {
            rdma_error_return(1, rdma_log_fp, "Cannot create QP\n");
        }
        ep->rc_ep.rc_qp[i].signaled_wr_id = 0;
        ep->rc_ep.rc_qp[i].send_count = 0;
        ep->rc_ep.rc_qp[i].state = RC_QP_ACTIVE;
        
        //rc = rc_qp_reset_to_init(ep, i);
        //if (0 != rc) {
        //    rdma_error_return(1, rdma_log_fp, "Cannot move QP to init state\n");
        //}
    }

    return 0;
}

/* ================================================================== */
/* Handle QPs state */

/** 
 * Connect a certain QP with a server: RESET->INIT->RTR->RTS
 * used for server arrivals
 */
int rc_connect_server( uint8_t idx, int qp_id )
{
    int rc;
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    ib_ep_t *ep = (ib_ep_t*)RDMA_DATA->config.servers[idx].ep;
    
    //ev_tstamp start_ts = ev_now(RDMA_DATA->loop);
    
    ibv_query_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_STATE, &init_attr);
    if (attr.qp_state != IBV_QPS_RESET) {
        rc = rc_qp_reset(ep, qp_id);
        if (0 != rc) {
            rdma_error_return(1, rdma_log_fp, "Cannot move QP to reset state\n");
        }
    }
    
    rc = rc_qp_reset_to_init(ep, qp_id);
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "Cannot move QP to init state\n");
    }
    rc = rc_qp_init_to_rtr(ep, qp_id);
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "Cannot move QP to RTR state\n");
    }
    rc = rc_qp_rtr_to_rts(ep, qp_id);
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "Cannot move QP to RTS state\n");
    }

    //info_wtime(rdma_log_fp, " # Connect server: %lf (ms)\n", (ev_now(RDMA_DATA->loop) - start_ts)*1000);
    return 0;
}

/* ================================================================== */

/**
 * Move a QP to the RESET state 
 */
static int
rc_qp_reset( ib_ep_t *ep, int qp_id )
{
    int rc;
    //struct ibv_wc wc;
    struct ibv_qp_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RESET;
    //while (ibv_poll_cq(IBDEV->rc_cq[qp_id], 1, &wc) > 0);
    //empty_completion_queue(0, qp_id, 0, NULL);
    rc = ibv_modify_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_STATE); 
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "ibv_modify_qp failed because %s\n", strrdma_error(rc));
    }
    
    return 0;
}

/**
 * Transit a QP from RESET to INIT state 
 */
static int 
rc_qp_reset_to_init( ib_ep_t *ep, int qp_id )
{
    int rc;
    struct ibv_qp_attr attr;
//    struct ibv_qp_init_attr init_attr;
//    ibv_query_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_STATE, &init_attr);
//    info(rdma_log_fp, "[QP Info (LID=%"PRIu16")] %s QP: %s\n",
//         ep->ud_ep.lid, (LOG_QP == qp_id) ? "log" : "ctrl",
//         qp_state_to_str(attr.qp_state));
    
    /* Reset state */
    ep->rc_ep.rc_qp[qp_id].signaled_wr_id = 0;
    ep->rc_ep.rc_qp[qp_id].send_count = 0;
    ep->rc_ep.rc_qp[qp_id].state = RC_QP_ACTIVE;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = IBDEV->port_num;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | 
                           IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_ATOMIC |
                           IBV_ACCESS_LOCAL_WRITE;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, 
                        IBV_QP_STATE | IBV_QP_PKEY_INDEX | 
                        IBV_QP_PORT | IBV_QP_ACCESS_FLAGS); 
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "ibv_modify_qp failed because %s\n", strrdma_error(rc));
    }
    return 0;
}

/**
 * Transit a QP from INIT to RTR state 
 */
static int 
rc_qp_init_to_rtr( ib_ep_t *ep, int qp_id )
{
    int rc;
    struct ibv_qp_attr attr;
    uint32_t psn;
    if (LOG_QP == qp_id) {
        uint64_t term = SID_GET_TERM(RDMA_DATA->cached_sid);
        psn = (uint32_t)(term & 0xFFFFFF);
    }
    //struct ibv_qp_init_attr init_attr;
    //uint8_t max_dest_rd_atomic;
    //ibv_query_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_MAX_DEST_RD_ATOMIC, &init_attr);
    //max_dest_rd_atomic = attr.max_dest_rd_atomic;

    /* Move the QP into the RTR state */
    memset(&attr, 0, sizeof(attr));
    attr.qp_state           = IBV_QPS_RTR;
    /* Setup attributes */
    attr.path_mtu           = IBDEV->mtu;
    attr.max_dest_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;
    attr.min_rnr_timer      = 12;
    attr.dest_qp_num        = ep->rc_ep.rc_qp[qp_id].qpn;
    attr.rq_psn             = (LOG_QP == qp_id) ? psn : CTRL_PSN;
    /* Note: this needs to modified for the lock; see rc_log_qp_lock */
    attr.ah_attr.is_global     = 0;
    attr.ah_attr.dlid          = ep->ud_ep.lid;
    attr.ah_attr.port_num      = IBDEV->port_num;
    attr.ah_attr.sl            = 0;
    attr.ah_attr.src_path_bits = 0;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, 
                        IBV_QP_STATE | IBV_QP_PATH_MTU |
                        IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | 
                        IBV_QP_RQ_PSN | IBV_QP_AV | IBV_QP_DEST_QPN);
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "ibv_modify_qp failed because %s\n", strrdma_error(rc));
    }
    
    //debug(rdma_log_fp, "Move %s QP of lid=%"PRIu16" into RTR state RQ_PSN=%"PRIu32"\n", 
    //               log ? "log":"ctrl", ep->ud_ep.lid, attr.rq_psn);
    return 0;
}

/**
 * Transit a QP from RTR to RTS state
 */
static int 
rc_qp_rtr_to_rts( ib_ep_t *ep, int qp_id )
{
    int rc;
    struct ibv_qp_attr attr;
    uint32_t psn;
    if (LOG_QP == qp_id) {
        uint64_t term = SID_GET_TERM(RDMA_DATA->cached_sid);
        psn = (uint32_t)(term & 0xFFFFFF);
    }
    //struct ibv_qp_init_attr init_attr;
    //ibv_query_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_MAX_DEST_RD_ATOMIC, &init_attr);
    //info_wtime(rdma_log_fp, "RC QP[%s] max_rd_atomic=%"PRIu8"; max_dest_rd_atomic=%"PRIu8"\n", qp_id == LOG_QP ? "LOG" : "CTRL", attr.max_rd_atomic, attr.max_dest_rd_atomic);

    /* Move the QP into the RTS state */
    memset(&attr, 0, sizeof(attr));
    attr.qp_state       = IBV_QPS_RTS;
    //attr.timeout        = 5;    // ~ 131 us
    attr.timeout        = 1;    // ~ 8 us
    attr.retry_cnt      = 0;    // max is 7
    attr.rnr_retry      = 7;
    attr.sq_psn         = (LOG_QP == qp_id) ? psn : CTRL_PSN; // PSN for send queue
//debug(rdma_log_fp, "MY SQ PSN: %"PRIu32"\n", attr.sq_psn);
    attr.max_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, 
                        IBV_QP_STATE | IBV_QP_TIMEOUT |
                        IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | 
                        IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    if (0 != rc) {
        rdma_error_return(1, rdma_log_fp, "ibv_modify_qp failed because %s\n", strrdma_error(rc));
    }
    
    //ibv_query_qp(ep->rc_ep.rc_qp[qp_id].qp, &attr, IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_MAX_DEST_RD_ATOMIC, &init_attr);
    //info_wtime(rdma_log_fp, "RC QP[%s] max_rd_atomic=%"PRIu8"; max_dest_rd_atomic=%"PRIu8"\n", qp_id == LOG_QP ? "LOG" : "CTRL", attr.max_rd_atomic, attr.max_dest_rd_atomic);

    
    //debug(rdma_log_fp, "Move %s QP of lid=%"PRIu16" into RTS state SQ_PSN=%"PRIu32"\n", 
    //               qp_id==LOG_QP ? "log":"ctrl", ep->ud_ep.lid, attr.sq_psn);
    return 0;
}

/* ================================================================== */
/* Handle RDMA operations */

void rdma_write(uint8_t target, void* buf, uint32_t len, uint32_t offset){
    ib_ep_t *ep;
    rem_mem_t rm;

    ep = (ib_ep_t*)RDMA_DATA->config.servers[target].ep;

    if (0 == ep->rc_connected) 
    {
        posted_sends = -1;  // insuccess
        continue;
    }

    rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
    rm.rkey = ep->rc_ep.rmt_mr.rkey;
    posted_sends = 1;

    post_send(target, LOG_QP, buf, len, );
}

/**
 * Post send operation
 */
static int 
post_send( uint8_t server_id,
           int qp_id,
           void *buf,
           uint32_t len,
           struct ibv_mr *mr,
           enum ibv_wr_opcode opcode,
           int signaled,
           rem_mem_t rm,
           int *posted_sends )
{
    int wait_signaled_wr = 0;
    uint32_t *send_count_ptr;
    uint64_t *signaled_wrid_ptr;
    uint8_t  *qp_state_ptr;
    ib_ep_t *ep;
    struct ibv_sge sg;
    struct ibv_send_wr wr;
    struct ibv_send_wr *bad_wr;

    /* Define some temporary variables */
    ep = (ib_ep_t*)RDMA_DATA->config.servers[server_id].ep;
    send_count_ptr = &(ep->rc_ep.rc_qp.send_count);
    signaled_wrid_ptr = &(ep->rc_ep.rc_qp.signaled_wr_id);
    qp_state_ptr = &(ep->rc_ep.rc_qp.state);
    
    if (RC_QP_BLOCKED == *qp_state_ptr) {
        /* This QP is blocked; need to wait for the signaled WR */
        //empty_completion_queue(server_id, qp_id, 1, NULL);
    }
    if (RC_QP_rdma_error == *qp_state_ptr) {
        /* This QP is in ERR state - restart it */
        //rc_qp_restart(ep, qp_id);
        *qp_state_ptr = RC_QP_ACTIVE;
        *send_count_ptr = 0;
    }
    
    /* Increment number of posted sends to avoid QP overflow */
    (*send_count_ptr)++;
 
    /* Local memory */
    memset(&sg, 0, sizeof(sg));
    sg.addr   = (uint64_t)buf;
    sg.length = len;
    sg.lkey   = mr->lkey;
 
    memset(&wr, 0, sizeof(wr));
    WRID_SET_SSN(wr.wr_id, ssn);
    WRID_SET_CONN(wr.wr_id, server_id);

    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = opcode;
    if ( (*signaled_wrid_ptr != 0) && (WRID_GET_TAG(*signaled_wrid_ptr) == 0) ) 
    {
        /* Signaled WR was found */
        *signaled_wrid_ptr = 0;
    }
    if ( (*send_count_ptr == IBDEV->rc_max_send_wr >> 2) && (*signaled_wrid_ptr == 0) ) {
        /* A quarter of the Send Queue is full, add a special signaled WR*/
        wr.send_flags |= IBV_SEND_SIGNALED;
        WRID_SET_TAG(wr.wr_id);    // special mark
        *signaled_wrid_ptr = wr.wr_id;
        *send_count_ptr = 0;
    }
    else if (*send_count_ptr == 3 * IBDEV->rc_max_send_wr >> 2) {
        if (*signaled_wrid_ptr != 0) {
            /* The Send Queue is full; need to wait for the signaled WR */
            wait_signaled_wr = 1;
        }
    }
    if (signaled) {
        wr.send_flags |= IBV_SEND_SIGNALED;
    }  
    wr.wr.rdma.remote_addr = rm.raddr;
    wr.wr.rdma.rkey        = rm.rkey;
    ibv_post_send(ep->rc_ep.rc_qp.qp, &wr, &bad_wr);
    
    empty_completion_queue(server_id, qp_id, wait_signaled_wr, posted_sends);
    
    return 0;
}