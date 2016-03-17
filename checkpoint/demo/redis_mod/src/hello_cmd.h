// hello_cmd
#ifndef __HELLO_CMD_H
#define __HELLO_CMD_H
/*
//#jingle#write_hook#add
*/

typedef struct OutputHashNode{
    uint64_t index;
    uint64_t hashval;
}OutputHashNode; 

typedef struct Singleton_OutputHash{
    int hook_enable; 
    int diversity_enable;
    uint64_t output_index;
    uint64_t last_hash; 
    list* hash_list;
}Singleton_OutputHash; 


Singleton_OutputHash* get_OutputHash_instance();

ssize_t write_hooked(int fd, const void *buf, size_t count);

#endif /* __HELLO_CMD_H*/
