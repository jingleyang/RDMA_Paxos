#include "../include/rdma/dare_server.h"
#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_ibv_rc.h"
#include <byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

extern dare_ib_device_t *dare_ib_device;
#define IBDEV dare_ib_device
#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

/* ================================================================== */

static int rc_prerequisite();
static int rc_memory_reg();
static void rc_memory_dereg();
static void rc_qp_destroy(dare_ib_ep_t* ep);
static void rc_cq_destroy(dare_ib_ep_t* ep);
static int rc_connect_server();
static int sock_sync_data(int sock, int xfer_size, char *local_data, char *remote_data);
static int connect_qp(int sock);
static int rc_qp_init_to_rtr(dare_ib_ep_t *ep, uint16_t dlid, uint8_t *dgid);
static int rc_qp_rtr_to_rts(dare_ib_ep_t *ep);
static int rc_qp_reset_to_init( dare_ib_ep_t *ep);
static int poll_cq(int max_wc, struct ibv_cq *cq);

/* ================================================================== */

int rc_init()
{
    int rc;

    rc = rc_prerequisite();
    if (0 != rc) {
        error_return(1, log_fp, "Cannot create RC prerequisite\n");
    }

    /* Register memory for RC */
    rc = rc_memory_reg();
    if (0 != rc) {
        error_return(1, log_fp, "Cannot register RC memory\n");
    }

    rc = rc_connect_server();
    if (0 != rc)
    {
        error_return(1, log_fp, "Cannot connect servers\n");
    }
    
    return 0;
}

void rc_free()
{
    int i;
    if (NULL != SRV_DATA) {
        for (i = 0; i < SRV_DATA->config.len; i++) {
            dare_ib_ep_t* ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
            rc_qp_destroy(ep);      
            rc_cq_destroy(ep);   
        }
    }

    if (NULL != IBDEV->rc_pd) {
        ibv_dealloc_pd(IBDEV->rc_pd);
    }
    rc_memory_dereg();
}

static void rc_memory_dereg()
{
    int rc;
    
    if (NULL != IBDEV->lcl_mr) {
        rc = ibv_dereg_mr(IBDEV->lcl_mr);
        if (0 != rc) {
            rdma_error(log_fp, "Cannot deregister memory");
        }
        IBDEV->lcl_mr = NULL;
    }
}

static void rc_qp_destroy(dare_ib_ep_t* ep)
{
    int rc;

    if (NULL == ep) return;

    rc = ibv_destroy_qp(ep->rc_ep.rc_qp.qp);
    if (0 != rc) {
        rdma_error(log_fp, "ibv_destroy_qp failed because %s\n", strerror(rc));
    }
    ep->rc_ep.rc_qp.qp = NULL;

}

static void rc_cq_destroy(dare_ib_ep_t* ep)
{
    int rc;

    if (NULL == ep) return;

    rc = ibv_destroy_cq(ep->rc_ep.rc_cq.cq);
    if (0 != rc) {
        rdma_error(log_fp, "ibv_destroy_cq failed because %s\n", strerror(rc));
    }
    ep->rc_ep.rc_cq.cq = NULL;
}

static int rc_connect_server()
{
    uint32_t i, count = SRV_DATA->config.idx;
    int rc = 1, sockfd = -1;
    for (i = 0; count > 0; i++)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            fprintf(stderr, "ERROR opening socket\n");
            goto rc_connect_server_exit;
        }
        if (i == SRV_DATA->config.idx)
            continue;
        while (connect(sockfd, (struct sockaddr *)SRV_DATA->config.servers[i].peer_address, sizeof(struct sockaddr_in)) < 0);
        if (connect_qp(sockfd))
        {
            fprintf(stderr, "failed to connect QPs\n");
            goto rc_connect_server_exit;
        }
        if (close(sockfd))
        {
            fprintf(stderr, "failed to close socket\n");
            goto rc_connect_server_exit;
        }
        count--;
    }

    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    int newsockfd = -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(sockfd, (struct sockaddr *)SRV_DATA->config.servers[SRV_DATA->config.idx].peer_address, sizeof(struct sockaddr_in)) < 0)
    {
        perror ("ERROR on binding");
        goto rc_connect_server_exit;
    }
    listen(sockfd, 5);

    count = SRV_DATA->config.cid.size - SRV_DATA->config.idx;
    while (count > 1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connect_qp(newsockfd))
        {
            fprintf(stderr, "failed to connect QPs\n");
            goto rc_connect_server_exit;
        }
        if (close(newsockfd))
        {
            fprintf(stderr, "failed to close socket\n");
            goto rc_connect_server_exit;
        }
        count--;    
    }
    if (close(sockfd))
    {
        fprintf(stderr, "failed to close socket\n");
        goto rc_connect_server_exit;
    }

    rc = 0;
rc_connect_server_exit:
    return rc;
}

static int sock_sync_data(int sock, int xfer_size, char *local_data, char *remote_data) {
    int rc;
    int read_bytes = 0;
    int total_read_bytes = 0;
    rc = write(sock, local_data, xfer_size);
    if(rc < xfer_size)
        fprintf(stderr, "Failed writing data during sock_sync_data\n");
    else
        rc = 0;
    while(!rc && total_read_bytes < xfer_size) {
        read_bytes = read(sock, remote_data, xfer_size);
        if(read_bytes > 0)
            total_read_bytes += read_bytes;
        else
            rc = read_bytes;
    }
    return rc;
}

static int connect_qp(int sock)
{
    struct cm_con_data_t local_con_data;
    struct cm_con_data_t remote_con_data;
    struct cm_con_data_t tmp_con_data;
    int rc = 0;
    union ibv_gid my_gid;

    if (IBDEV->gid_idx >= 0)
    {
        rc = ibv_query_gid(IBDEV->ib_dev_context, IBDEV->port_num, IBDEV->gid_idx, &my_gid);
        if (rc)
        {
            fprintf(stderr, "could not get gid for port %d, index %d\n", IBDEV->port_num, IBDEV->gid_idx);
            return rc;
        }
    }
    else
        memset(&my_gid, 0, sizeof my_gid);

    local_con_data.idx = htonl(SRV_DATA->config.idx);
    local_con_data.log_mr.raddr = htonll((uintptr_t)IBDEV->lcl_mr->addr);
    local_con_data.log_mr.rkey = htonl(IBDEV->lcl_mr->rkey);

    struct ibv_qp_init_attr qp_init_attr;
    struct ibv_qp *tmp_qp;
    struct ibv_cq *tmp_cq;

    tmp_cq = ibv_create_cq(IBDEV->ib_dev_context, IBDEV->rc_cqe, NULL, NULL, 0);
    if (NULL == tmp_cq) {
        error_return(1, log_fp, "Cannot create CQ\n");
    }
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.send_cq = tmp_cq;
    qp_init_attr.recv_cq = tmp_cq;
    qp_init_attr.cap.max_inline_data = IBDEV->rc_max_inline_data;
    qp_init_attr.cap.max_send_sge = 1;  
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_wr = IBDEV->rc_max_send_wr;;
    tmp_qp = ibv_create_qp(IBDEV->rc_pd, &qp_init_attr);
    if (NULL == tmp_qp) {
        error_return(1, log_fp, "Cannot create QP\n");
    }

    local_con_data.qpns = htonl(tmp_qp->qp_num);

    local_con_data.lid = htons(IBDEV->lid);
    memcpy(local_con_data.gid, &my_gid, 16);
    fprintf(stdout, "\nLocal LID = 0x%x\n", IBDEV->lid);

    if (sock_sync_data(sock, sizeof(struct cm_con_data_t), (char*)&local_con_data, (char*)&tmp_con_data))
    {
        fprintf(stderr, "failed to exchange connection data between sides\n");
        rc = 1;
        goto connect_qp_exit;
    }

    remote_con_data.lid = ntohs(tmp_con_data.lid);
    memcpy(remote_con_data.gid, tmp_con_data.gid, 16);
    uint32_t idx = ntohl(tmp_con_data.idx);

    dare_ib_ep_t *ep = (dare_ib_ep_t*)SRV_DATA->config.servers[idx].ep;

    ep->rc_ep.rmt_mr.raddr = ntohll(tmp_con_data.log_mr.raddr);
    ep->rc_ep.rmt_mr.rkey = ntohl(tmp_con_data.log_mr.rkey);
    ep->rc_ep.rc_qp.qpn = ntohl(tmp_con_data.qpns);

    fprintf(stderr, "Node id = %"PRIu32"\n", idx);
    fprintf(stdout, "Remote LOG address = 0x%"PRIx64"\n", ep->rc_ep.rmt_mr.raddr);
    fprintf(stdout, "Remote LOG rkey = 0x%x\n", ep->rc_ep.rmt_mr.rkey);

    fprintf(stdout, "Remote LOG QP number = 0x%x\n", ep->rc_ep.rc_qp.qpn);

    fprintf(stdout, "Remote LID = 0x%x\n", remote_con_data.lid);
    if (IBDEV->gid_idx >= 0)
    {
        uint8_t *p = remote_con_data.gid;
        fprintf(stdout, "Remote GID = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }

    ep->rc_ep.rc_qp.qp = tmp_qp;
    ep->rc_ep.rc_cq.cq = tmp_cq;

    rc = rc_qp_reset_to_init(ep);
    if (rc)
    {
        fprintf(stderr, "change LOG QP state to INIT failed\n");
        goto connect_qp_exit;
    }

    rc = rc_qp_init_to_rtr(ep, remote_con_data.lid, remote_con_data.gid);
    if (rc)
    {
        fprintf(stderr, "failed to modify LOG QP state to RTR\n");
        goto connect_qp_exit;
    }

    rc = rc_qp_rtr_to_rts(ep);
    if (rc)
    {
        fprintf(stderr, "failed to modify LOG QP state to RTS\n");
        goto connect_qp_exit;
    }
    fprintf(stdout, "LOG QP state was change to RTS\n");

    ep->rc_connected = 1;

    connect_qp_exit:
    return rc;
}

static int rc_prerequisite()
{
    IBDEV->rc_max_send_wr = IBDEV->ib_dev_attr.max_qp_wr;
    info(log_fp, "# IBDEV->rc_max_send_wr = %"PRIu32"\n", IBDEV->rc_max_send_wr);
    
    IBDEV->rc_cqe = IBDEV->ib_dev_attr.max_cqe;
    info(log_fp, "# IBDEV->rc_cqe = %d\n", IBDEV->rc_cqe);
    
    /* Allocate a RC protection domain */
    IBDEV->rc_pd = ibv_alloc_pd(IBDEV->ib_dev_context);
    if (NULL == IBDEV->rc_pd) {
        error_return(1, log_fp, "Cannot create PD\n");
    }

    if (0 != find_max_inline(IBDEV->ib_dev_context, IBDEV->rc_pd, &IBDEV->rc_max_inline_data))
    {
        error_return(1, log_fp, "Cannot find max RC inline data\n");
    }
    info(log_fp, "# MAX_INLINE_DATA = %"PRIu32"\n", IBDEV->rc_max_inline_data);
    
    return 0;
}

static int rc_memory_reg()
{  
    /* Register memory for local log */    
    IBDEV->lcl_mr = ibv_reg_mr(IBDEV->rc_pd, SRV_DATA->log, sizeof(dare_log_t) + SRV_DATA->log->len, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE);
    if (NULL == IBDEV->lcl_mr) {
        error_return(1, log_fp, "Cannot register memory because %s\n", strerror(errno));
    }
    
    return 0;
}

static int rc_qp_reset_to_init(dare_ib_ep_t *ep)
{
    int rc;
    struct ibv_qp_attr attr;

    ep->rc_ep.rc_qp.send_count = 0;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = IBDEV->port_num;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | 
    IBV_ACCESS_REMOTE_READ |
    IBV_ACCESS_REMOTE_ATOMIC |
    IBV_ACCESS_LOCAL_WRITE;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS); 
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    return 0;
}

static int rc_qp_init_to_rtr(dare_ib_ep_t *ep, uint16_t dlid, uint8_t *dgid)
{
    int rc;
    struct ibv_qp_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state           = IBV_QPS_RTR;

    attr.path_mtu           = IBDEV->mtu;
    attr.max_dest_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;
    attr.min_rnr_timer      = 12;
    attr.dest_qp_num        = ep->rc_ep.rc_qp.qpn;
    attr.rq_psn             = 0;

    attr.ah_attr.is_global     = 0;
    attr.ah_attr.dlid          = dlid;
    attr.ah_attr.port_num      = IBDEV->port_num;
    attr.ah_attr.sl            = 0;
    attr.ah_attr.src_path_bits = 0;

    if (IBDEV->gid_idx >= 0)
    {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = IBDEV->gid_idx;
        attr.ah_attr.grh.traffic_class = 0;
    }

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | IBV_QP_RQ_PSN | IBV_QP_AV | IBV_QP_DEST_QPN);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    
    return 0;
}

static int rc_qp_rtr_to_rts(dare_ib_ep_t *ep)
{
    int rc;
    struct ibv_qp_attr attr;

    /* Move the QP into the RTS state */
    memset(&attr, 0, sizeof(attr));
    attr.qp_state       = IBV_QPS_RTS;
    //attr.timeout        = 5;    // ~ 131 us
    attr.timeout        = 1;    // ~ 8 us
    attr.retry_cnt      = 0;    // max is 7
    attr.rnr_retry      = 7;
    attr.sq_psn         = 0;
    attr.max_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, 
        IBV_QP_STATE | IBV_QP_TIMEOUT |
        IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | 
        IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    return 0;
}

int post_send(uint32_t server_id, void *buf, uint32_t len, struct ibv_mr *mr, enum ibv_wr_opcode opcode, rem_mem_t rm)
{
    int rc;
    uint32_t *send_count_ptr;
    dare_ib_ep_t *ep;
    struct ibv_sge sg;
    struct ibv_send_wr wr;
    struct ibv_send_wr *bad_wr;

    /* Define some temporary variables */
    ep = (dare_ib_ep_t*)SRV_DATA->config.servers[server_id].ep;
    send_count_ptr = &(ep->rc_ep.rc_qp.send_count);

    memset(&sg, 0, sizeof(sg));
    sg.addr   = (uint64_t)buf;
    sg.length = len;
    sg.lkey   = mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = opcode;

    if((*send_count_ptr & S_DEPTH_) == 0) {
        wr.send_flags |= IBV_SEND_SIGNALED; /* Specifying IBV_SEND_SIGNALED in wr.send_flags indicates that we want completion notification for this send request */
    }
    if ((*send_count_ptr & S_DEPTH_) == S_DEPTH_)
    {
        poll_cq(1, ep->rc_ep.rc_cq.cq);
    }

    if (IBV_WR_RDMA_WRITE == opcode) {
        if (len <= IBDEV->rc_max_inline_data) {
            wr.send_flags |= IBV_SEND_INLINE;
        }
    }   

    wr.wr.rdma.remote_addr = rm.raddr;
    wr.wr.rdma.rkey        = rm.rkey;
    rc = ibv_post_send(ep->rc_ep.rc_qp.qp, &wr, &bad_wr);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_post_send failed because %s [%s]\n", 
            strerror(rc), rc == EINVAL ? "EINVAL" : rc == ENOMEM ? "ENOMEM" : rc == EFAULT ? "EFAULT" : "UNKNOWN");
    }

    (*send_count_ptr)++;

    return 0;
}

static int poll_cq(int max_wc, struct ibv_cq *cq)
{
    struct ibv_wc wc[1];
    int ret = -1, i, total_wc = 0;
    do {
        ret = ibv_poll_cq(cq, max_wc - total_wc, wc + total_wc);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to poll cq for wc due to %d \n", ret);
            return ret;
        }
        total_wc += ret;
    } while(total_wc < max_wc);
    fprintf(stdout, "%d WC are completed \n", total_wc);
    for (i = 0; i < total_wc; i++)
    {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "Work completion (WC) has error status: %d (means: %s) at index %d\n", -wc[i].status, ibv_wc_status_str(wc[i].status), i);
            return -(wc[i].status);
        }
    }
    return total_wc;
}