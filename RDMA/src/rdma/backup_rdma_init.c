#include "../include/rdma/rdma_common.h"

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *client_cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp;

static struct ibv_mr *client_metadata_mr = NULL, *client_buffer_mr = NULL, *server_metadata_mr = NULL;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_sge client_send_sge, server_recv_sge;

static int client_prepare_connection(struct sockaddr_in *s_addr)
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;

	cm_event_channel = rdma_create_event_channel();
	if (!cm_event_channel) {
		rdma_error("Creating cm event channel failed, errno: %d \n", -errno);
		return -errno;
	}
	rdma_debug("RDMA CM event channel is created at : %p \n", cm_event_channel);

	ret = rdma_create_id(cm_event_channel, &cm_client_id, NULL, RDMA_PS_TCP);
	if (ret) {
		rdma_error("Creating cm id failed with errno: %d \n", -errno); 
		return -errno;
	}

	ret = rdma_resolve_addr(cm_client_id, NULL, (struct sockaddr*) s_addr, 2000);
	if (ret) {
		rdma_error("Failed to resolve address, errno: %d \n", -errno);
		return -errno;
	}
	rdma_debug("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");

	ret  = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ADDR_RESOLVED, &cm_event);
	if (ret) {
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}

	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		rdma_error("Failed to acknowledge the CM event, errno: %d\n", -errno);
		return -errno;
	}
	rdma_debug("RDMA address is resolved \n");

	ret = rdma_resolve_route(cm_client_id, 2000);
	if (ret) {
		rdma_error("Failed to resolve route, erno: %d \n", -errno);
	       return -errno;
	}
	rdma_debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");

	ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ROUTE_RESOLVED, &cm_event);
	if (ret) {
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}

	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		rdma_error("Failed to acknowledge the CM event, errno: %d \n", -errno);
		return -errno;
	}
	printf("Trying to connect to server at : %s port: %d \n", inet_ntoa(s_addr->sin_addr), ntohs(s_addr->sin_port));

	pd = ibv_alloc_pd(cm_client_id->verbs);
	if (!pd) {
		rdma_error("Failed to alloc pd, errno: %d \n", -errno);
		return -errno;
	}
	rdma_debug("pd allocated at %p \n", pd);

	io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
	if (!io_completion_channel) {
		rdma_error("Failed to create IO completion event channel, errno: %d\n", -errno);
		return -errno;
	}
	rdma_debug("completion event channel created at : %p \n", io_completion_channel);

	client_cq = ibv_create_cq(cm_client_id->verbs, CQ_CAPACITY, NULL, io_completion_channel, 0);
	if (!client_cq) {
		rdma_error("Failed to create CQ, errno: %d \n", -errno);
		return -errno;
	}
	rdma_debug("CQ created at %p with %d elements \n", client_cq, client_cq->cqe);

	ret = ibv_req_notify_cq(client_cq, 0);
	if (ret) {
		rdma_error("Failed to request notifications, errno: %d\n", -errno);
		return -errno;
	}

    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE;
    qp_init_attr.cap.max_recv_wr = MAX_WR;
    qp_init_attr.cap.max_send_sge = MAX_SGE;
    qp_init_attr.cap.max_send_wr = MAX_WR;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.recv_cq = client_cq;
    qp_init_attr.send_cq = client_cq;
    ret = rdma_create_qp(cm_client_id, pd, &qp_init_attr);
	if (ret) {
		rdma_error("Failed to create QP, errno: %d \n", -errno);
	       return -errno;
	}
	client_qp = cm_client_id->qp;
	rdma_debug("QP created at %p \n", client_qp);
	return 0;
}

static int client_pre_post_recv_buffer()
{
	int ret = -1;
	server_metadata_mr = rdma_buffer_register(pd, &server_metadata_attr, sizeof(server_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));
	if(!server_metadata_mr){
		rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
		return -ENOMEM;
	}
	server_recv_sge.addr = (uint64_t) server_metadata_mr->addr;
	server_recv_sge.length = (uint32_t) server_metadata_mr->length;
	server_recv_sge.lkey = (uint32_t) server_metadata_mr->lkey;

	bzero(&server_recv_wr, sizeof(server_recv_wr));
	server_recv_wr.sg_list = &server_recv_sge;
	server_recv_wr.num_sge = 1;
	ret = ibv_post_recv(client_qp, &server_recv_wr, &bad_server_recv_wr);
	if (ret) {
		rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
		return ret;
	}
	rdma_debug("Receive buffer pre-posting is successful \n");
	return 0;
}

static int client_connect_to_server() 
{
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	bzero(&conn_param, sizeof(conn_param));
	conn_param.initiator_depth = 3;
	conn_param.responder_resources = 3;
	conn_param.retry_count = 3;
	ret = rdma_connect(cm_client_id, &conn_param);
	if (ret) {
		rdma_error("Failed to connect to remote host , errno: %d\n", -errno);
		return -errno;
	}
	rdma_debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
	ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
	if (ret) {
		rdma_error("Failed to get cm event, ret = %d \n", ret);
	       return ret;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		rdma_error("Failed to acknowledge cm event, errno: %d\n", 
			       -errno);
		return -errno;
	}
	printf("The client is connected successfully \n");
	return 0;
}

static int client_send_metadata_to_server() 
{
	struct ibv_wc wc[2];
	int ret = -1;

	client_buffer_mr = rdma_buffer_alloc(pd, LOG_SIZE, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
	
	if(!client_buffer_mr){
		rdma_error("Failed to register the first buffer, ret = %d \n", ret);
		return ret;
	}

	client_metadata_attr.address = (uint64_t) client_buffer_mr->addr; 
	client_metadata_attr.length = client_buffer_mr->length; 
	client_metadata_attr.stag.local_stag = client_buffer_mr->lkey;

	client_metadata_mr = rdma_buffer_register(pd, &client_metadata_attr, sizeof(client_metadata_attr), IBV_ACCESS_LOCAL_WRITE);
	if(!client_metadata_mr) {
		rdma_error("Failed to register the client metadata buffer, ret = %d \n", ret);
		return ret;
	}

	client_send_sge.addr = (uint64_t) client_metadata_mr->addr;
	client_send_sge.length = (uint32_t) client_metadata_mr->length;
	client_send_sge.lkey = client_metadata_mr->lkey;

	bzero(&client_send_wr, sizeof(client_send_wr));
	client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_SEND;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;

	ret = ibv_post_send(client_qp, &client_send_wr, &bad_client_send_wr);
	if (ret) {
		rdma_error("Failed to send client metadata, errno: %d \n", -errno);
		return -errno;
	}

	ret = process_work_completion_events(io_completion_channel, wc, 2);
	if(ret != 2) {
		rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
		return ret;
	}
	rdma_debug("Server sent us its buffer location and credentials, showing \n");
	show_rdma_buffer_attr(&server_metadata_attr);
	return 0;
}

int backup_rdma_init(uint32_t group_size, struct sockaddr_in* server_sockaddr, node_id_t leader_id) {
	int ret;
	ret = client_prepare_connection(server_sockaddr);
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	 }
	ret = client_pre_post_recv_buffer(); 
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	}
	ret = client_connect_to_server();
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	}
	ret = client_send_metadata_to_server();
	if (ret) {
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	}

	srv_data.metadata_attr[leader_id] = server_metadata_attr;
	srv_data.buffer_mr = client_buffer_mr;
	srv_data.qp[leader_id] = client_cq;
	srv_data.tail = 0;
	return ret;
}