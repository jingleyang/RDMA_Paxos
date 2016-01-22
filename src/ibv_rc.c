extern ib_device_t *ib_device;
#define IBDEV ib_device
#define SRV_DATA ((server_data_t*)ib_device->udata)

/**
 * Initialize RC data
 */
int rc_init()
{

    rc_prerequisite();

    /* Register memory for RC */
    rc_memory_reg();
    
    /* Create QPs for RC communication */
    for (i = 0; i < SRV_DATA->config.len; i++) {
        ib_ep_t* ep = (ib_ep_t*)
                        SRV_DATA->config.servers[i].ep;
        
        /* Create QPs for this endpoint */
        rc_qp_create(ep);
    }
    
    return 0;
}

int rc_prerequisite()
{   
    /* Allocate a RC protection domain */
    IBDEV->rc_pd = ibv_alloc_pd(IBDEV->ib_dev_context);

    return 0;
}

int rc_memory_reg()
{  
    /* Register memory for local log */    
    IBDEV->lcl_mr = ibv_reg_mr(IBDEV->rc_pd,
            SRV_DATA->log, sizeof(log_t) + SRV_DATA->log->len, 
            IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC | 
            IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE);

    return 0;
}

int rc_qp_create(ib_ep_t* ep )
{
    int i;
    struct ibv_qp_init_attr qp_init_attr;
    
    if (NULL == ep) return 0;

    /* Create RC QP */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.send_cq = IBDEV->rc_cq;
    qp_init_attr.recv_cq = IBDEV->rc_cq;
    qp_init_attr.cap.max_inline_data = IBDEV->rc_max_inline_data;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_wr = IBDEV->rc_max_send_wr;
    ep->rc_ep.rc_qp.qp = ibv_create_qp(IBDEV->rc_pd, &qp_init_attr);

    ep->rc_ep.rc_qp.signaled_wr_id = 0;
    ep->rc_ep.rc_qp.send_count = 0;
    ep->rc_ep.rc_qp.state = RC_QP_ACTIVE;

    return 0;
}

void RDMA_write(void* buf, uint32_t len, uint32_t offset, uint8_t target){
    rem_mem_t rm;
    rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
    rm.rkey = ep->rc_ep.rmt_mr.rkey;
    if (target == 0)
    {
        uint8_t size = get_group_size(SRV_DATA->config);
        for (int i = 0; i < size; i++){
            if (i == SRV_DATA->config.idx){
                continue;
            }
            if (!CID_IS_SERVER_ON(SRV_DATA->config.cid, i)){
                continue;
            }

            ep = (ib_ep_t*)SRV_DATA->config.servers[i].ep;

            post_send(i, buf, len, IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);
        }
    }else{
            ep = (ib_ep_t*)SRV_DATA->config.servers[target].ep;

            post_send(i, buf, len, IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);
    }
}

/* ================================================================== */
/* Handle RDMA operations */

/**
 * Post send operation
 */
static int 
post_send( uint8_t server_id,
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
    ep = (ib_ep_t*)SRV_DATA->config.servers[server_id].ep;
    send_count_ptr = &(ep->rc_ep.rc_qp.send_count);
    signaled_wrid_ptr = &(ep->rc_ep.rc_qp.signaled_wr_id);
    qp_state_ptr = &(ep->rc_ep.rc_qp.state);
    
    if (RC_QP_BLOCKED == *qp_state_ptr) {
        /* This QP is blocked; need to wait for the signaled WR */
        //empty_completion_queue(server_id, qp_id, 1, NULL);
    }
    if (RC_QP_ERROR == *qp_state_ptr) {
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