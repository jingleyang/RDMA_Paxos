// hello_cmd
#ifndef __HELLO_CMD_H
#define __HELLO_CMD_H
/*
//#jingle#write_hook#add
*/
#define BUFF_SIZE 512

typedef struct OutputHashNode{
    uint64_t index; // The round number
    uint64_t hashval; // The hash of the output
    char     owner[BUFF_SIZE]; // The name of the owner, which is used for debuging.
}OutputHashNode; 

typedef struct Singleton_OutputHash{
    int hook_enable;  // A boolean value to indicate whether enable hooked function.
    int diversity_enable; // A boolean value to force introduce a hashvalue error.
    uint64_t diversity_hash; // A error of hash value which is used for show error case.
    uint64_t output_index; // A index which indicates the current round number.
    uint64_t last_hash; // The hash value of the master.
    list* hash_list; // The list which stores all hash node of the master.
    list* hash_collect; // The list which stores all hash node received from salves.
}Singleton_OutputHash; 


Singleton_OutputHash* get_OutputHash_instance(); // To get instance through this API.

ssize_t write_hooked(int fd, const void *buf, size_t count); // To hook write function.

#endif /* __HELLO_CMD_H*/
