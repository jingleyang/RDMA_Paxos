#ifndef DEBUG_H
#define DEBUG_H

#define debug_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu.%06lu:",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define err_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu.%06lu:",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define rec_log(out,args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf((out),"%lu.%06lu:",tv.tv_sec,tv.tv_usec); \
    fprintf((out),args); \
    fflush(out); \
}while(0);

#define safe_rec_log(x,args...) {if(NULL!=(x)){rec_log((x),args);}}

#define SYS_LOG(x,args...) {if((x)->sys_log){safe_rec_log(((x)->sys_log_file),args)}}

#define STAT_LOG(x,args...) {if((x)->stat_log){safe_rec_log(((x)->sys_log_file),args)}}

#define REQ_LOG(x,args...) {if((x)->req_log){safe_rec_log(((x)->sys_log_file),args)}}

#define rdma_error(msg, args...) do {\
	fprintf(stderr, "%s : %d : ERROR : "msg, __FILE__, __LINE__, ## args);\
}while(0);

#ifdef ACN_RDMA_DEBUG 
#define rdma_debug(msg, args...) do {\
    printf("DEBUG: "msg, ## args);\
}while(0);

#else 

#define rdma_debug(msg, args...) 

#endif /* ACN_RDMA_DEBUG */


#endif