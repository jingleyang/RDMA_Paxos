#define REQ_MAJORITY 13

/* ================================================================== */
/* UD messages */

struct ud_hdr_t {
    uint64_t id;
    uint8_t type;
    //uint8_t pad[7];
};
typedef struct ud_hdr_t ud_hdr_t;

struct reconf_rep_t {
    ud_hdr_t   hdr;
    uint8_t    idx;
    dare_cid_t cid;
    uint64_t cid_idx;
    uint64_t head;
};
typedef struct reconf_rep_t reconf_rep_t;

struct rc_syn_t {
    ud_hdr_t hdr;
    rem_mem_t log_rm;
    uint8_t idx;
    uint8_t data[0];    // log QPNs
};
typedef struct rc_syn_t rc_syn_t;

struct rc_ack_t {
	ud_hdr_t hdr;
	uint8_t idx;
};
typedef struct rc_ack_t rc_ack_t;