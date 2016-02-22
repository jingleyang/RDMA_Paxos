#define CID_IS_SERVER_ON(cid, idx) ((cid).bitmask & (1 << (idx)))

/** 
 * Configuration ID: A configuration is given by a 
 * [N, STATE, BITMASK] tuple, where:
 * N - is the group size
 * STATE - is the configuration state: stable, transitional, extended
 * BITMASK - is a bitmask with a bit set for every on servers
 *
 * A configuration can be in three states:
 * a stable state that entails a group of P servers with the non-faulty servers indicated by a bitmask
 * an extended state used for adding servers to a full group
 * a transitional state that allows for the group to be resized without interrupting normal operation
 */
struct cid_t
{
    uint8_t size;
    uint32_t bitmask;
    uint8_t state;
};
typedef struct cid_t cid_t;

typedef struct server_t server_t;
struct rdma_config_t 
{
	cid_t cid;         		/* configuration identifier */

    server_t *servers;      /* array with info for each server */

    uint8_t idx;            /* own index in configuration */
    uint8_t len;            /* fixed length of configuration array */
};
typedef struct rdma_config_t rdma_config_t;