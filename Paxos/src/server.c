static int (*c_recv)(int,void*,size_t,int);

/* ssize_t recv(int sockfd, void *buf, size_t len, int flags) */
recv(int sockfd, void *buf, size_t len, int flags)
{
	// We need to call the real recv first
	if (!((void*)c_recv=dlsym(RTLD_NEXT,"recv")))
	{
		//Fail to get the original recv
	}
	c_recv(sockfd, buf, len, flags);

    char* config_path = "";
    char* sys_log = "";
    uint32_t node_id = 0;
    char* start_mode = '';
    struct consensus_component* consensus_comp = init_consensus_comp(config_path, sys_log, node_id, start_mode);

	// initialize RDMA
    rdma_init(consensus_comp->node_id, consensus_comp->group_size, consensus_comp->sys_log_file);
	// TODO: only for the first time

	if (consensus_comp->my_role == LEADER)
	{
		rsm_op(consensus_comp, sockfd, buf, len);
	}

	//return to process_req(buf) and then sned back
}