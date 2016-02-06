#define info(stream, fmt, ...) do {\
    fprintf(stream, fmt, ##__VA_ARGS__); \
    fflush(stream); \
} while(0)
#define info_wtime(stream, fmt, ...) do {\
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

//#ifdef DEBUG
#define error(stream, fmt, ...) do { \
    fprintf(stream, "[ERROR] %s/%d/%s() " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stream); \
} while(0)
//#else
//#define error(stream, fmt, ...)
//#endif

//#ifdef DEBUG
#define error_return(rc, stream, fmt, ...) do { \
    fprintf(stream, "[ERROR] %s/%d/%s() " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stream); \
    return (rc);  \
} while(0)
//#else
//#define error_return(rc, stream, fmt, ...) return (rc)
//#endif