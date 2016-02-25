#include "../include/rdma/rdma_common.h"

dare_server_data_t srv_data;

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp = NULL;

static struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL, *server_metadata_mr = NULL;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;

static int rc_prerequisite(struct sockaddr_in *server_addr)
{
    int ret = -1;

    cm_event_channel = rdma_create_event_channel();
    if (!cm_event_channel) {
        rdma_error("Creating cm event channel failed with errno : (%d)", -errno);
        return -errno;
    }
    rdma_debug("RDMA CM event channel is created successfully at %p \n", cm_event_channel);

    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    if (ret) {
        rdma_error("Creating server cm id failed with errno: %d ", -errno);
        return -errno;
    }
    rdma_debug("A RDMA connection id for the server is created \n");

    ret = rdma_bind_addr(cm_server_id, (struct sockaddr*) server_addr);
    if (ret) {
        rdma_error("Failed to bind server address, errno: %d \n", -errno);
        return -errno;
    }
    rdma_debug("Server RDMA CM id is successfully binded \n");

    io_completion_channel = ibv_create_comp_channel(cm_server_id->verbs);
    if (!io_completion_channel) {
        rdma_error("Failed to create an I/O completion event channel, %d\n", -errno);
        return -errno;
    }
    rdma_debug("An I/O completion event channel is created at %p \n", io_completion_channel);

    cq = ibv_create_cq(cm_server_id->verbs, CQ_CAPACITY, NULL, io_completion_channel, 0);
    if (!cq) {
        rdma_error("Failed to create a completion queue (cq), errno: %d\n", -errno);
        return -errno;
    }
    rdma_debug("Completion queue (CQ) is created at %p with %d elements \n", cq, cq->cqe);

    ret = ibv_req_notify_cq(cq, 0); // do we need this?
    if (ret) {
        rdma_error("Failed to request notifications on CQ errno: %d \n", -errno);
        return -errno;
    }

    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE;
    qp_init_attr.cap.max_recv_wr = MAX_WR;
    qp_init_attr.cap.max_send_sge = MAX_SGE;
    qp_init_attr.cap.max_send_wr = MAX_WR;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.send_cq = cq;

    pd = ibv_alloc_pd(cm_server_id->verbs);
    if (!pd) {
        rdma_error("Failed to allocate a protection domain errno: %d\n", -errno);
        return -errno;
    }
    rdma_debug("A new protection domain is allocated at %p \n", pd);

    server_buffer_mr = rdma_buffer_alloc(pd, LOG_SIZE, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ| IBV_ACCESS_REMOTE_WRITE));

    server_metadata_attr.address = (uint64_t) server_buffer_mr->addr; 
    server_metadata_attr.length = server_buffer_mr->length; 
    server_metadata_attr.stag.local_stag = server_buffer_mr->lkey;

    server_metadata_mr = rdma_buffer_register(pd, &server_metadata_attr, sizeof(server_metadata_attr), IBV_ACCESS_LOCAL_WRITE);

    if(!server_metadata_mr) {
        rdma_error("Failed to register the server metadata buffer, ret = %d \n", ret);
        return ret;
    }
    
    /* this is a non-blocking call */
    ret = rdma_listen(cm_server_id, 8);
    if (ret) {
        rdma_error("rdma_listen failed to listen on server address, errno: %d ", -errno);
        return -errno;
    }
    printf("Server is listening successfully at: %s , port: %d \n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port));

    return 0;
}

static int start_rdma_server()
{
    struct rdma_cm_event *cm_event = NULL;

    /* We wait (block) on the connection management event channel for the connect event */
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
    if (ret) {
        rdma_error("Failed to get cm event, ret = %d \n" , ret);
        return ret;
    }

    cm_client_id = cm_event->id;

    ret = rdma_ack_cm_event(cm_event);
    if (ret) {
        rdma_error("Failed to acknowledge the cm event errno: %d \n", -errno);
        return -errno;
    }
    rdma_debug("A new RDMA client connection id is stored at %p\n", cm_client_id);
}

static int setup_client_resources()
{
    int ret = -1;

    ret = rdma_create_qp(cm_client_id, pd, &qp_init_attr);
    if (ret) {
        rdma_error("Failed to create QP due to errno: %d\n", -errno);
        return -errno;
    }
    client_qp = cm_client_id->qp;
    rdma_debug("Client QP created at %p\n", client_qp);
    return ret;
}

static int accept_client_connection()
{
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    struct sockaddr_in remote_sockaddr; 
    int ret = -1;

    client_metadata_mr = rdma_buffer_register(pd, &client_metadata_attr, sizeof(client_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));

    if(!client_metadata_mr){
        rdma_error("Failed to register client attr buffer\n");
        return -ENOMEM;
    }

    client_recv_sge.addr = (uint64_t) client_metadata_mr->addr;
    client_recv_sge.length = client_metadata_mr->length;
    client_recv_sge.lkey = client_metadata_mr->lkey;

    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1;
    ret = ibv_post_recv(client_qp, &client_recv_wr, &bad_client_recv_wr);
    if (ret) {
        rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
        return ret;
    }
    rdma_debug("Receive buffer pre-posting is successful \n");

    memset(&conn_param, 0, sizeof(conn_param));

    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;
    ret = rdma_accept(cm_client_id, &conn_param);
    if (ret) {
        rdma_error("Failed to accept the connection, errno: %d \n", -errno);
        return -errno;
    }
    rdma_debug("Going to wait for : RDMA_CM_EVENT_ESTABLISHED event \n");
    
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
    if (ret) {
        rdma_error("Failed to get the cm event, errnp: %d \n", -errno);
        return -errno;
    }

    ret = rdma_ack_cm_event(cm_event);
    if (ret) {
        rdma_error("Failed to acknowledge the cm event %d\n", -errno);
        return -errno;
    }

    /* How to extract connection information */
    memcpy(&remote_sockaddr, rdma_get_peer_addr(cm_client_id), sizeof(struct sockaddr_in));
    printf("A new connection is accepted from %s \n", inet_ntoa(remote_sockaddr.sin_addr));
    return ret;
}

static int send_server_metadata_to_client() 
{
    struct ibv_wc wc;
    int ret = -1;

    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    if (ret != 1) {
        rdma_error("Failed to receive , ret = %d \n", ret);
        return ret;
    }

    printf("Client side buffer information is received...\n");
    show_rdma_buffer_attr(&client_metadata_attr);
    printf("The client has requested buffer length of : %u bytes \n", client_metadata_attr.length);

    server_send_sge.addr = (uint64_t)server_metadata_mr->addr;
    server_send_sge.length = server_metadata_mr->length;
    server_send_sge.lkey = server_metadata_mr->lkey;

    bzero(&server_send_wr, sizeof(server_send_wr));

    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1;
    server_send_wr.opcode = IBV_WR_SEND;
    server_send_wr.send_flags = IBV_SEND_SIGNALED;
    /* Now we post it */
    ret = ibv_post_send(client_qp, &server_send_wr, &bad_server_send_wr);
    if (ret) {
        rdma_error("Failed to send server metadata, errno: %d \n", -errno);
        return -errno;
    }

    return ret;
}

int primary_rdma_init(uint32_t group_size, struct sockaddr_in server_sockaddr, node_id_t node_id)
{
    rc_prerequisite(&server_sockaddr);

    uint32_t i;
    for (i = 0; i < group_size; ++i)
    {
        if (i == node_id)
            continue;
        start_rdma_server();
        accept_client_connection();
        send_server_metadata_to_client();
        srv_data.qp[i] = client_qp;
        srv_data.metadata_attr[i] = client_metadata_attr;
        cm_client_id++;
        client_qp++;
        client_metadata_mr++;
    }
    srv_data.buffer_mr = server_buffer_mr;
    srv_data.tail = 0;
    return 0;
}