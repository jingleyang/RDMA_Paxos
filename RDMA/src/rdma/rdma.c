#include "../include/rdma/rdma_common.h"

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *cq = NULL, *client_cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp = NULL;

static struct ibv_mr *client_metadata_mr = NULL, *log_buffer_mr = NULL, *server_metadata_mr = NULL;
static void *log_buffer;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;

static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;

static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;

static struct ibv_sge client_recv_sge, client_send_sge, server_recv_sge, server_send_sge;

static node_id_t myid;

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
	ret = find_max_inline(cm_client_id->verbs, pd, &srv_data.rc_max_inline_data);
    if (ret != 0)
    {
    	rdma_error("Cannot find max RC inline data, ret = %d \n", ret);
    	return ret;
    }
    rdma_debug("MAX_INLINE_DATA = %"PRIu32"\n", srv_data.rc_max_inline_data);

    /* The capacity here is define statically but this can be probed from the 
     * device. We just use a small number as defined in rdma_common.h */
    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE;
    qp_init_attr.cap.max_recv_wr = Q_DEPTH;
    qp_init_attr.cap.max_send_sge = MAX_SGE;
    qp_init_attr.cap.max_send_wr = Q_DEPTH; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap.max_inline_data = srv_data.rc_max_inline_data;

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
		rdma_error("Failed to acknowledge cm event, errno: %d\n", -errno);
		return -errno;
	}
	printf("The client is connected successfully \n");
	return 0;
}

static int client_send_metadata_to_server() 
{
	struct ibv_wc wc[2];
	int ret = -1;
	log_buffer_mr = rdma_buffer_alloc(pd, LOG_SIZE, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
	if(!log_buffer_mr){
		rdma_error("Failed to register the first buffer, ret = %d \n", ret);
		return ret;
	}

	client_metadata_attr.address = (uint64_t) log_buffer_mr->addr; 
	client_metadata_attr.length = log_buffer_mr->length; 
	client_metadata_attr.buf_rkey = log_buffer_mr->rkey;
	client_metadata_attr.node_id = myid;


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

static int setup_client_resources()
{
	int ret = -1;
	if(!cm_client_id){
		rdma_error("Client id is still NULL \n");
		return -EINVAL;
	}

	pd = ibv_alloc_pd(cm_client_id->verbs);
	if (!pd) {
		rdma_error("Failed to allocate a protection domain errno: %d\n", -errno);
		return -errno;
	}
	rdma_debug("A new protection domain is allocated at %p \n", pd);

	io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
	if (!io_completion_channel) {
		rdma_error("Failed to create an I/O completion event channel, %d\n",
				-errno);
		return -errno;
	}
	rdma_debug("An I/O completion event channel is created at %p \n", io_completion_channel);

	cq = ibv_create_cq(cm_client_id->verbs, CQ_CAPACITY, NULL, io_completion_channel, 0);
	if (!cq) {
		rdma_error("Failed to create a completion queue (cq), errno: %d\n", -errno);
		return -errno;
	}
	rdma_debug("Completion queue (CQ) is created at %p with %d elements \n", cq, cq->cqe);

	ret = ibv_req_notify_cq(cq, 0);
	if (ret) {
		rdma_error("Failed to request notifications on CQ errno: %d \n", -errno);
		return -errno;
	}

	ret = find_max_inline(cm_client_id->verbs, pd, &srv_data.rc_max_inline_data);
    if (ret != 0)
    {
    	rdma_error("Cannot find max RC inline data, ret = %d \n", ret);
    	return ret;
    }

	bzero(&qp_init_attr, sizeof qp_init_attr);
	qp_init_attr.cap.max_recv_sge = MAX_SGE;
	qp_init_attr.cap.max_recv_wr = Q_DEPTH;
	qp_init_attr.cap.max_send_sge = MAX_SGE;
	qp_init_attr.cap.max_send_wr = Q_DEPTH;
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.cap.max_inline_data = srv_data.rc_max_inline_data;

	qp_init_attr.recv_cq = cq;
	qp_init_attr.send_cq = cq;

	ret = rdma_create_qp(cm_client_id, pd, &qp_init_attr);
	if (ret) {
		rdma_error("Failed to create QP due to errno: %d\n", -errno);
		return -errno;
	}

	client_qp = cm_client_id->qp;
	rdma_debug("Client QP created at %p\n", client_qp);
	return ret;
}

/* Starts an RDMA server by allocating basic connection resources */
static int start_rdma_server(struct sockaddr_in *server_addr) 
{
	struct rdma_cm_event *cm_event = NULL;
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

	ret = rdma_listen(cm_server_id, 8);
	if (ret) {
		rdma_error("rdma_listen failed to listen on server address, errno: %d ",
				-errno);
		return -errno;
	}
	printf("Server is listening successfully at: %s , port: %d \n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port));

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
	return ret;
}

static int accept_client_connection()
{
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;
	struct sockaddr_in remote_sockaddr; 
	int ret = -1;
	if(!cm_client_id || !client_qp) {
		rdma_error("Client resources are not properly setup\n");
		return -EINVAL;
	}

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

	/* The same memory buffer can be registered several times (even with different access permissions) and every registration results in a different set of keys */
	log_buffer_mr = rdma_buffer_register(pd, log_buffer, LOG_SIZE, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));

	server_metadata_attr.address = (uint64_t) log_buffer_mr->addr; 
	server_metadata_attr.length = log_buffer_mr->length; 
	server_metadata_attr.buf_rkey = log_buffer_mr->rkey;
	server_metadata_attr.node_id = myid;

	server_metadata_mr = rdma_buffer_register(pd, &server_metadata_attr, sizeof(server_metadata_attr), IBV_ACCESS_LOCAL_WRITE);
	if(!server_metadata_mr) {
		rdma_error("Failed to register the client metadata buffer, ret = %d \n", ret);
		return ret;
	}

	server_send_sge.addr = (uint64_t) server_metadata_mr->addr;
	server_send_sge.length = (uint32_t) server_metadata_mr->length;
	server_send_sge.lkey = server_metadata_mr->lkey;

	bzero(&server_send_wr, sizeof(server_send_wr));
	server_send_wr.sg_list = &server_send_sge;
	server_send_wr.num_sge = 1;
	server_send_wr.opcode = IBV_WR_SEND;
	server_send_wr.send_flags = IBV_SEND_SIGNALED;

	ret = ibv_post_send(client_qp, &server_send_wr, &bad_server_send_wr);
	if (ret) {
		rdma_error("Failed to send client metadata, errno: %d \n", -errno);
		return -errno;
	}

	return 0;
}

static void *event_thread(void *arg)
{
	int ret;
	struct sockaddr_in *server_sockaddr = (struct sockaddr_in *)arg;
	while (1)
	{
		ret = start_rdma_server(server_sockaddr);
		if (ret) {
			rdma_error("RDMA server failed to start cleanly, ret = %d \n", ret);
		}
		ret = setup_client_resources();
		if (ret) { 
			rdma_error("Failed to setup client resources, ret = %d \n", ret);
		}
		ret = accept_client_connection();
		if (ret) {
			rdma_error("Failed to handle client cleanly, ret = %d \n", ret);
		}
		ret = send_server_metadata_to_client();
		if (ret) {
			rdma_error("Failed to send server metadata to the client, ret = %d \n", ret);
		}
		srv_data.qp[client_metadata_attr.node_id] = client_qp;
		srv_data.metadata_attr[client_metadata_attr.node_id] = client_metadata_attr;
		srv_data.local_key[client_metadata_attr.node_id] = log_buffer_mr->lkey;
		srv_data.cq[client_metadata_attr.node_id] = cq;

		srv_data.log_mr = log_buffer_mr->addr;
	}
	return 0;
}

int connect_peers(struct sockaddr_in *server_addr, int is_leader, node_id_t node_id)
{
	int ret;
	myid = node_id;
	if (is_leader)
	{
		log_buffer = calloc(1, LOG_SIZE);
		pthread_t cm_thread;

		pthread_create(&cm_thread, NULL, &event_thread, server_addr);

		return 0;
	}else{
		ret = client_prepare_connection(server_addr);
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
		srv_data.qp[server_metadata_attr.node_id] = client_qp;
		srv_data.local_key[server_metadata_attr.node_id] = log_buffer_mr->lkey;
		srv_data.metadata_attr[server_metadata_attr.node_id] = server_metadata_attr;
		srv_data.cq[server_metadata_attr.node_id] = client_cq;
		srv_data.log_mr = log_buffer_mr->addr;
		return ret;
	}
}