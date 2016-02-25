#ifndef DEBUG_H
#define DEBUG_H

#define con_err_log(args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf(stderr,"%lu.%06lu:",tv.tv_sec,tv.tv_usec); \
    fprintf(stderr,args); \
}while(0);

#define rec_con_log(out,args...) do { \
    struct timeval tv; \
    gettimeofday(&tv,0); \
    fprintf((out),"%lu.%06lu:",tv.tv_sec,tv.tv_usec); \
    fprintf((out),args); \
    fflush(out); \
}while(0);

#define safe_rec_con_log(x,args...) {if(NULL!=(x)){rec_con_log((x),args);}}

#define CON_LOG(x,args...) {safe_rec_con_log(((x)->con_log_file),args)}

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