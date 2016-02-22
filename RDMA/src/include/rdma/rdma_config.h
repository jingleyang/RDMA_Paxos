#ifndef RDMA_CONFIG_H
#define RDMA_CONFIG_H 

#define CID_IS_SERVER_ON(cid, idx) ((cid).bitmask & (1 << (idx)))
#define CID_SERVER_ADD(cid, idx) (cid).bitmask |= 1 << (idx)

/** 
 * BITMASK - is a bitmask with a bit set for every on servers
 */
struct cid_t
{
    uint8_t size;
    uint32_t bitmask;
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

#endif /* RDMA_CONFIG_H */