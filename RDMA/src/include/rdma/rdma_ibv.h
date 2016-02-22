#include <infiniband/verbs.h> /* OFED IB verbs */

#define mtu_value(mtu) \
    ((mtu == IBV_MTU_256) ? 256 :    \
    (mtu == IBV_MTU_512) ? 512 :    \
    (mtu == IBV_MTU_1024) ? 1024 :  \
    (mtu == IBV_MTU_2048) ? 2048 :  \
    (mtu == IBV_MTU_4096) ? 4096 : 0)

#define LOG_QP 1
#define CTRL_QP 0

struct ud_ep_t {
    uint16_t lid;
    uint32_t qpn;
    struct ibv_ah *ah;
};
typedef struct ud_ep_t ud_ep_t;

struct rem_mem_t {
    uint64_t raddr;
    uint32_t rkey;
};
typedef struct rem_mem_t rem_mem_t;

#define RC_QP_ACTIVE    0
#define RC_QP_BLOCKED   1
#define RC_QP_ERROR     2

struct rc_qp_t {
    struct ibv_qp *qp;          // RC QP
    uint64_t signaled_wr_id;    // ID of signaled WR (to avoid overflow)
    uint32_t send_count;        // number of posted sends
    uint8_t  state;             // QP's state
};
typedef struct rc_qp_t rc_qp_t;

/* Endpoint RC info */
struct rc_ep_t {
    rem_mem_t rmt_mr;    // remote memory regions
};
typedef struct rc_ep_t rc_ep_t;

struct ib_ep_t {
    rc_ep_t rc_ep;  // RC info
    rc_qp_t   rc_qp;
    int rc_connected; /* rc_connected = 1 to mark RC established */
};
typedef struct ib_ep_t ib_ep_t;

struct ib_device_t{
	/* General fields */
    struct ibv_device *ib_dev;
    struct ibv_context *ib_dev_context;
    struct ibv_device_attr ib_dev_attr;
    uint16_t pkey_index;    
    uint8_t port_num;       // port number
    uint32_t mtu;           // MTU (Maximum Transmission Unit) for this device

    /* UD */
    struct ibv_pd           *ud_pd;
    struct ibv_qp           *ud_qp;
    struct ibv_cq           *ud_rcq;
    struct ibv_cq           *ud_scq;
    int                     ud_rcqe;
    void                    **ud_recv_bufs;
    struct ibv_mr           **ud_recv_mrs;
    void                    *ud_send_buf;
    struct ibv_mr           *ud_send_mr;
    uint32_t                ud_max_inline_data;
    uint64_t  request_id;

    /* Multicast */
    struct ibv_ah *ib_mcast_ah;
    union ibv_gid mgid;
    uint16_t      mlid;

    /* QPs for inter-server communication - RC */
    struct ibv_pd *rc_pd;
    struct ibv_cq *rc_cq[2]; //just in case we need CTRL_QP in the future
    int           rc_cqe;
    struct ibv_wc *rc_wc_array;
    struct ibv_mr *lcl_mr[2]; //just in case we need CTRL_QP in the future
    uint32_t      rc_max_send_wr;
    uint32_t      rc_max_inline_data;

    void *udata;
};
typedef struct ib_device_t ib_device_t;

/* ================================================================== */

int init_ib_device();
int start_ib_ud();
int init_ib_srv_data( void *data );
int init_ib_rc();

int ib_join_cluster();
int ib_exchange_rc_info();