#include "../include/rdma/rdma_common.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

static struct resources res;

struct configuration_t config =
{
	NULL, /* dev_name (default first device found) */
	1, /* ib_port (default 1) */
	-1, /* gid_idx (default not used) */
	-1 /* node id*/
};

static void resources_init(struct resources *res)
{
	memset(res, 0, sizeof *res);
	res->sock = -1;
}

static int resources_create(struct resources *res)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *ib_dev = NULL;

	size_t size;
	int i;
	int mr_flags = 0;
	int cq_size = 0;
	int num_devices;
	int rc = 0;

	dev_list = ibv_get_device_list(&num_devices);
	if (!dev_list)
	{
		fprintf(stderr, "failed to get IB devices list\n"); rc = 1;
		goto resources_create_exit;
	}

	if (!num_devices)
	{
		fprintf(stderr, "found %d devices\n", num_devices);
		rc = 1;
		goto resources_create_exit;
	}

	fprintf(stdout, "found %d device(s)\n", num_devices);

	for (i = 0; i < num_devices; i ++)
	{
		if(!config.dev_name) {
			config.dev_name = strdup(ibv_get_device_name(dev_list[i]));
			fprintf(stdout, "device not specified, using first one found: %s\n", config.dev_name);
		}

		if (!strcmp(ibv_get_device_name(dev_list[i]), config.dev_name))
		{
			ib_dev = dev_list[i];
			break;
		}
	}

	if (!ib_dev)
	{
		fprintf(stderr, "IB device %s wasn't found\n", config.dev_name);
		rc = 1;
		goto resources_create_exit;
	}
	/* get device handle */
	res->ib_ctx = ibv_open_device(ib_dev);
	if (!res->ib_ctx)
	{
		fprintf(stderr, "failed to open device %s\n", config.dev_name);
		rc = 1;
		goto resources_create_exit;
	}

	/* We are now done with device list, free it */
	ibv_free_device_list(dev_list);
	dev_list = NULL;
	ib_dev = NULL;

	/* query port properties */
	if (ibv_query_port(res->ib_ctx, config.ib_port, &res->port_attr)) {
		fprintf(stderr, "ibv_query_port on port %u failed\n", config.ib_port);
		rc = 1;
		goto resources_create_exit;
	}

	/* allocate Protection Domain */
	res->pd = ibv_alloc_pd(res->ib_ctx);
	if (!res->pd)
	{
		fprintf(stderr, "ibv_alloc_pd failed\n");
		rc = 1;
		goto resources_create_exit;
	}

	/* Completion Queue with CQ_CAPACITY entry */
	cq_size = CQ_CAPACITY;
	res->cq = ibv_create_cq(res->ib_ctx, cq_size, NULL, NULL, 0);
	if (!res->cq)
	{
		fprintf(stderr, "failed to create CQ with %u entries\n", cq_size);
		rc = 1;
		goto resources_create_exit;
	}

	size = LOG_SIZE;
	res->buf = (char*)malloc(size);
	if (!res->buf)
	{
		fprintf(stderr, "failed to malloc %Zu bytes to memory buffer\n", size);
		rc = 1;
		goto resources_create_exit;
	}

	memset(res->buf, 0, size);

	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
	res->mr = ibv_reg_mr(res->pd, res->buf, size, mr_flags);
	if (!res->mr)
	{
		fprintf(stderr, "ibv_reg_mr failed with mr_flags=0x%x\n", mr_flags);
		rc = 1;
		goto resources_create_exit;
	}
	fprintf(stdout, "MR was registered with addr=%p, lkey=0x%x, rkey=0x%x, flags=0x%x\n", res->buf, res->mr->lkey, res->mr->rkey, mr_flags);	


    if (0 != find_max_inline(res->ib_ctx, res->pd, &res->rc_max_inline_data))
    {
        fprintf(stderr, "Cannot find max RC inline data\n");
    }
    fprintf(stdout, "# MAX_INLINE_DATA = %"PRIu32"\n", res->rc_max_inline_data);

resources_create_exit:
	if (rc)
	{
		/* Error encountered, cleanup */
		if(res->mr)
		{
			ibv_dereg_mr(res->mr);
			res->mr = NULL;
		}

		if(res->buf)
		{
			free(res->buf);
			res->buf = NULL;
		}
		if(res->cq)
		{
			ibv_destroy_cq(res->cq);
			res->cq = NULL;
		}

		if (res->pd)
		{
			ibv_dealloc_pd(res->pd);
			res->pd = NULL;
		}

		if (res->ib_ctx)
		{
			ibv_close_device(res->ib_ctx);
			res->ib_ctx = NULL;
		}

		if (dev_list)
		{
			ibv_free_device_list(dev_list);
			dev_list = NULL;
		}
	}
	return rc;
}


static int modify_qp_to_init(struct ibv_qp *qp)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_INIT;
	attr.port_num = config.ib_port;
	attr.pkey_index = 0;
	attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

	flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc)
		fprintf(stderr, "Failed to modify QP state to INIT\n");

	return rc;
}

static int modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid, uint8_t *dgid)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_256; /* Maximum Transmission Unit (MTU) supported by port. Can be: IBV_MTU_256, IBV_MTU_512, IBV_MTU_1024 */
	attr.dest_qp_num = remote_qpn;
	attr.rq_psn = 0;
	attr.max_dest_rd_atomic = 1; /* Number of responder resources for handling incoming RDMA reads & atomic operations (valid only for RC QPs) */
	attr.min_rnr_timer = 0x12;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.dlid = dlid;
	attr.ah_attr.sl = 0;
	attr.ah_attr.src_path_bits = 0;
	attr.ah_attr.port_num = config.ib_port;
	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc)
		fprintf(stderr, "failed to modify QP state to RTR\n");

	return rc;
}

static int modify_qp_to_rts(struct ibv_qp *qp)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 1; // ~ 8 us
	attr.retry_cnt = 0; // max is 7
	attr.rnr_retry = 7;
	attr.sq_psn = 0;
	attr.max_rd_atomic = 1;

	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc)
		fprintf(stderr, "failed to modify QP state to RTS\n");
	return rc;
}

int sock_sync_data(int sock, int xfer_size, char *local_data, char *remote_data) {
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

static int connect_qp(struct resources *res)
{
	struct cm_con_data_t local_con_data;
	struct cm_con_data_t remote_con_data;
	struct cm_con_data_t tmp_con_data;
	int rc = 0;
	union ibv_gid my_gid;

	local_con_data.node_id = config.node_id;
	local_con_data.addr = htonll((uintptr_t)res->buf);
	local_con_data.rkey = htonl(res->mr->rkey);

	/* create the Queue Pair */
	struct ibv_qp_init_attr qp_init_attr;
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));

	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.sq_sig_all = 1;
	qp_init_attr.send_cq = res->cq;
	qp_init_attr.recv_cq = res->cq;
	qp_init_attr.cap.max_send_wr = Q_DEPTH; /* Maximum send posting capacity */
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	struct ibv_qp *tmp_qp = ibv_create_qp(res->pd, &qp_init_attr);
	if (!tmp_qp)
	{
		fprintf(stderr, "failed to create QP\n");
		rc = 1;
		goto connect_qp_exit;
	}

	local_con_data.qp_num = htonl(tmp_qp->qp_num);
	local_con_data.lid = htons(res->port_attr.lid);
	memcpy(local_con_data.gid, &my_gid, 16);
	fprintf(stdout, "\nLocal LID = 0x%x\n", res->port_attr.lid);

	if (sock_sync_data(res->sock, sizeof(struct cm_con_data_t), (char*)&local_con_data, (char*)&tmp_con_data))
	{
		fprintf(stderr, "failed to exchange connection data between sides\n");
		rc = 1;
		goto connect_qp_exit;
	}

	remote_con_data.addr = ntohll(tmp_con_data.addr);
	remote_con_data.rkey = ntohl(tmp_con_data.rkey);
	remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
	remote_con_data.lid = ntohs(tmp_con_data.lid);
	memcpy(remote_con_data.gid, tmp_con_data.gid, 16);

	res->remote_props[tmp_con_data.node_id] = remote_con_data; /* values to connect to remote side */

	fprintf(stderr, "Node id = %"PRId64":\n", remote_con_data.node_id);
	fprintf(stdout, "Remote address = 0x%"PRIx64"\n", remote_con_data.addr);
	fprintf(stdout, "Remote rkey = 0x%x\n", remote_con_data.rkey);
	fprintf(stdout, "Remote QP number = 0x%x\n", remote_con_data.qp_num);
	fprintf(stdout, "Remote LID = 0x%x\n", remote_con_data.lid);

	res->qp[tmp_con_data.node_id] = tmp_qp;
	fprintf(stdout, "Local QP for %"PRId64" was created, QP number=0x%x\n", remote_con_data.node_id, res->qp[tmp_con_data.node_id]->qp_num);

	rc = modify_qp_to_init(res->qp[tmp_con_data.node_id]);
	if (rc)
	{
		fprintf(stderr, "change QP state to INIT failed\n");
		goto connect_qp_exit;
	}
	rc = modify_qp_to_rtr(res->qp[tmp_con_data.node_id], remote_con_data.qp_num, remote_con_data.lid, remote_con_data.gid);
	if (rc)
	{
		fprintf(stderr, "failed to modify QP state to RTR\n");
		goto connect_qp_exit;
	}

	rc = modify_qp_to_rts(res->qp[tmp_con_data.node_id]);
	if (rc)
	{
		fprintf(stderr, "failed to modify QP state to RTR\n");
		goto connect_qp_exit;
	}
	fprintf(stdout, "QP state was change to RTS\n");

connect_qp_exit:
	return rc;
}

void *connect_peers(peer* peer_pool, int64_t node_id, uint32_t group_size)
{
	config.node_id = node_id;
	
	resources_init(&res);

	if (resources_create(&res))
	{
		fprintf(stderr, "failed to create resources\n");
	}

	int i, sockfd = -1, count = node_id;
	for (i = 0; count > 0; i++)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			fprintf(stderr, "ERROR opening socket\n");
		if (i == config.node_id)
			continue;
		while (connect(sockfd, (struct sockaddr *)peer_pool[i].peer_address, sizeof(struct sockaddr_in)) < 0);
		res.sock = sockfd;
		if (connect_qp(&res))
		{
			fprintf(stderr, "failed to connect QPs\n");
		}
		if (close(sockfd))
		{
			fprintf(stderr, "failed to close socket\n");
		}
		count--;
	}

	struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof(clientaddr);
	int newsockfd = -1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(sockfd, (struct sockaddr *)peer_pool[node_id].peer_address, sizeof(struct sockaddr_in)) < 0)
	{
		perror ("ERROR on binding");
	}
	listen(sockfd, 5);

	count = group_size - node_id;
	while (count > 1)
	{
		newsockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
		res.sock = newsockfd;
		if (connect_qp(&res))
		{
			fprintf(stderr, "failed to connect QPs\n");
		}
		if (close(newsockfd))
		{
			fprintf(stderr, "failed to close socket\n");
		}
		count--;	
	}
	if (close(sockfd))
	{
		fprintf(stderr, "failed to close socket\n");
	}

	return &res;
}