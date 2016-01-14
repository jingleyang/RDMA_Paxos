struct rem_mem_t {
    uint64_t raddr;
    uint32_t rkey;
};

struct rc_qp_t {
    struct ibv_qp *qp;          // RC QP
    uint64_t signaled_wr_id;    // ID of signaled WR (to avoid overflow)
    uint32_t send_count;        // number of posted sends
    uint8_t  state;             // QP's state
};

/* Endpoint RC info */
struct rc_ep_t {
    rem_mem_t rmt_mr;    // remote memory regions
};

struct ib_ep_t {
    rc_ep_t rc_ep;  // RC info
    rc_qp_t   rc_qp;
};

struct ib_device_t{
	/* General fields */
    struct ibv_device *ib_dev;
    struct ibv_context *ib_dev_context;
    struct ibv_device_attr ib_dev_attr;
    uint16_t pkey_index;    
    uint8_t port_num;       // port number

    /* QPs for inter-server communication - RC */
    struct ibv_pd *rc_pd;
    struct ibv_cq *rc_cq;
    int           rc_cqe;
    struct ibv_wc *rc_wc_array;
    struct ibv_mr *lcl_mr;
    uint32_t      rc_max_send_wr;
    uint32_t      rc_max_inline_data;

    void *udata;
};