#include <debug.h>

#ifndef RDMA_H_
#define RDMA_H_

#define MAX_SERVER_COUNT 13

#define PAGE_SIZE 4096

/**
 *  UD message types 
 */
#define MSG_NONE 0
#define MSG_ERROR 13
/* Initialization messages */
#define RC_SYN      1
#define RC_SYNACK   2
#define RC_ACK      3
/* Config messages */
#define JOIN        211
#define CFG_REPLY   214

#endif /* RDMA_H_ */
