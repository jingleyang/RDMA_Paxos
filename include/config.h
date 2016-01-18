#define CID_IS_SERVER_ON(cid, idx) ((cid).bitmask & (1 << (idx)))

/** 
 * BITMASK - is a bitmask with a bit set for every on servers
 */
struct cid_t
{
	uint8_t size;
    uint32_t bitmask;
};

struct server_config_t 
{
	cid_t cid;         		/* configuration identifier */

    server_t *servers;      /* array with info for each server */

    uint8_t idx;            /* own index in configuration */
    uint8_t len;            /* fixed length of configuration array */
};

uint8_t get_group_size(server_config_t config)
{
    return config.cid.size;
}