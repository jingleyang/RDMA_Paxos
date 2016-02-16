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

#define rdma_info(stream, fmt, ...) do {\
    fprintf(stream, fmt, ##__VA_ARGS__); \
    fflush(stream); \
} while(0)
#define rdma_info_wtime(stream, fmt, ...) do {\
    struct timeval _debug_tv;\
    gettimeofday(&_debug_tv,NULL);\
/*    if (prev_tv.tv_sec != 0) { \
        double __tmp = (_debug_tv.tv_sec - prev_tv.tv_sec) * 1000 + (_debug_tv.tv_usec -  prev_tv.tv_usec)/1000;\
        if (__tmp > 15) {\
            jump_cnt++;\
            fprintf(stream, "Time jump (%lf) ms %"PRIu64"\n", __tmp, jump_cnt);\
        }\
    }*/\
    fprintf(stream, "[%lu:%06lu] " fmt, _debug_tv.tv_sec, _debug_tv.tv_usec, ##__VA_ARGS__); \
    fflush(stream); \
} while(0)

#define rdma_error(stream, fmt, ...) do { \
    fprintf(stream, "[ERROR] %s/%d/%s() " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stream); \
} while(0)

#define rdma_error_return(rc, stream, fmt, ...) do { \
    fprintf(stream, "[ERROR] %s/%d/%s() " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stream); \
    return (rc);  \
} while(0)

#endif