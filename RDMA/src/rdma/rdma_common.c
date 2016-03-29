#include "../include/rdma/rdma_common.h"

int find_max_inline(struct ibv_context *context, struct ibv_pd *pd, uint32_t *max_inline_arg)
{
    int rc;
    struct ibv_qp *qp = NULL;
    struct ibv_cq *cq = NULL;
    struct ibv_qp_init_attr init_attr;
    uint32_t max_inline_data;
    
    *max_inline_arg = 0;

    cq = ibv_create_cq(context, 1, NULL, NULL, 0);
    if (NULL == cq) {
        return 1;
    }
    
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.send_cq = cq;
    init_attr.recv_cq = cq;
    init_attr.srq = 0;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_recv_wr = 1;
    
    init_attr.cap.max_inline_data = max_inline_data = 1 << 20;
    rc = 1;
    while (max_inline_data > 0) {
        qp = ibv_create_qp(pd, &init_attr);
        if (NULL != qp) {
            *max_inline_arg = max_inline_data;
            ibv_destroy_qp(qp);
            rc = 0;
            break;
        }
        max_inline_data >>= 1;
        init_attr.cap.max_inline_data = max_inline_data;
    }
    
    if (NULL != cq) {
        ibv_destroy_cq(cq);
    }

    return rc;
}

int poll_cq(int max_wc, struct ibv_cq *cq)
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

int rdma_write(uint8_t target, void *buf, uint32_t len, uint64_t offset, void *udata)
{
	struct resources *res = (struct resources *)udata;
	struct ibv_send_wr sr;
	struct ibv_sge sge;
	struct ibv_send_wr *bad_wr = NULL;
	int rc;

	memset(&sge, 0, sizeof(sge));
	sge.addr   = (uint64_t)buf;
	sge.length = len;
	sge.lkey = res->mr->lkey;

	memset(&sr, 0, sizeof(sr));
	sr.sg_list = &sge;
	sr.num_sge = 1;
	sr.opcode = IBV_WR_RDMA_WRITE;

    if((res->req_num[target] & S_DEPTH_) == 0) {
    	sr.send_flags |= IBV_SEND_SIGNALED; /* Specifying IBV_SEND_SIGNALED in wr.send_flags indicates that we want completion notification for this send request */
    }
    if ((res->req_num[target] & S_DEPTH_) == S_DEPTH_)
    {
    	poll_cq(1, res->cq[target]);
    }

	if (len <= res->rc_max_inline_data)
		sr.send_flags |= IBV_SEND_INLINE;

	sr.wr.rdma.remote_addr = res->remote_props[target].addr + offset;
	sr.wr.rdma.rkey = res->remote_props[target].rkey;

	rc = ibv_post_send(res->qp[target], &sr, &bad_wr);
	if (rc)
		fprintf(stderr, "failed to post SR, rc = %d\n", rc);
	else
	{
		fprintf(stdout, "RDMA Write Request was posted\n");
	}
	res->req_num[target]++;

	return rc;
}