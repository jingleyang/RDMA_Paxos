/*
Date: 17 March, 2016
Author: Jingyu Yang
Description: A demo to show how to add a command for redis-server
Steps:
1. To Add files named hello_cmd.h/c
2. hello_cmd.h should be included in server.h
3. server.h should be included in hello_cmd.c 
4. export helloCommand declearation to server.h
5. Add hello_cmd.o to REDIS_SERVER_OBJ in the Makefile:120
6. make dep V=1 OPTIMIZATION="-O0" MALLOC="libc" 
7. To implement the function
*/

#include "server.h"
#include "hiredis.h" //redis c client API

void report_hashCommand(client *c){
    //serverLog(LL_DEBUG,"[report_hash] argc=%d, argv[0]=%s, argv[1]=%s, argv[2]=%s",c->argc,(char*)c->argv[0]->ptr, (char*)c->argv[1]->ptr,(char*)c->argv[2]->ptr);
    uint64_t index = strtoll((char*)c->argv[1]->ptr,NULL,10);
    uint64_t hashval = strtoll((char*)c->argv[2]->ptr,NULL,10);
    serverLog(LL_DEBUG,"[report_hash] recvied index:%"PRIu64", hashval:0x%"PRIx64,index,hashval);
    Singleton_OutputHash* handle = get_OutputHash_instance(); 
    const int BUFF_SIZE=512;
    char output_buff[BUFF_SIZE];
    snprintf(output_buff,BUFF_SIZE-1,"M_hash %"PRIu64" %"PRIu64,handle->output_index,handle->last_hash);
    addReplyBulkCString(c,output_buff);
}
void diversityCommand(client *c){
    serverLog(LL_DEBUG,"[data_diversity] will create a diversity hash at the next request"); 
    Singleton_OutputHash* handle = get_OutputHash_instance();    
    handle->diversity_enable=1;
    addReplyBulkCString(c,"OK");
}
void worldCommand(client *c){
    serverLog(LL_DEBUG,"[world_cmd] just ignore");
    addReply(c,shared.nullbulk); 
}
void helloCommand(client *c){
    serverLog(LL_DEBUG,"[hello_cmd] argc=%d, argv[0]=%s, argv[1]=%s",c->argc,(char*)c->argv[0]->ptr, (char*)c->argv[1]->ptr);
    // force passing command to slaves
    forceCommandPropagation(c,PROPAGATE_REPL);
    // force a slave reply the master.
    c->flags|=CLIENT_MASTER_FORCE_REPLY;
    if (2 != c->argc){ // argc error
        addReplyBulkCString(c,"cmd argc error");
    }else{
        // reply
        sds reply_msg = sdscatfmt(sdsempty(),"__WORLD__ %s",c->argv[1]->ptr);
        addReplyBulkSds(c,reply_msg);
    }
    // clear the flags
    c->flags &= ~CLIENT_MASTER_FORCE_REPLY;
}

Singleton_OutputHash* get_OutputHash_instance(){
    static Singleton_OutputHash * instance = NULL;
    if (NULL == instance){
        // do initialisation
        instance = malloc(sizeof(Singleton_OutputHash));
        instance->hook_enable = 0;
        instance->diversity_enable = 0;
        instance->output_index = -1; // because I will do inc firstly
        instance->last_hash = 0;
        instance->hash_list = listCreate(); 
    }else{
        //serverLog(LL_DEBUG,"[singleton] handle has been reused");
    }
    return instance;
}
void report_to_master(const char* hostname, int port, uint64_t index, uint64_t hashval){
    redisContext *c;
    redisReply *reply;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    reply = redisCommand(c,"report_hash %"PRIu64" %"PRIu64,index,hashval); 
    serverLog(LL_DEBUG,"[report_hash] %s",reply->str);
    freeReplyObject(reply);
    redisFree(c);
}
void report_hashval(uint64_t index, uint64_t hashval){
    serverLog(LL_DEBUG,"[report_hash] masterhost:%s, masterport:%d",server.masterhost,server.masterport);
    if (NULL == server.masterhost){
        serverLog(LL_DEBUG,"[report_hash] I am the master. I will not report hashval");
        return;
    }else{
        serverLog(LL_DEBUG,"[report_hash] I am a slave. I will report to the master");
        report_to_master(server.masterhost,server.masterport,index,hashval); 
    }
}
// TODO when the instance is destructed?
//ssize_t write(int fd, const void *buf, size_t count);
ssize_t write_hooked(int fd, const void *buf, size_t count){
    UNUSED(fd);
    char* tmp_buf = malloc(count+1);
    strncpy(tmp_buf,(const char*)buf,count);
    tmp_buf[count]='\0';
    Singleton_OutputHash* handle = get_OutputHash_instance();
    int found_hello = strstr(buf,"__WORLD__")!=NULL?1:0;
    if (found_hello){
        handle->hook_enable = 1;
    }else{
        handle->hook_enable = 0;
    }
    if (!handle->hook_enable){
        return 0;
    }
    handle->hook_enable = 0;
    size_t buffer_size = count;
    const char* buffer_ptr = buf;
    uint64_t buffer_hash=crc64(handle->last_hash,(const unsigned char*)buffer_ptr,buffer_size);
    if (handle->diversity_enable){
        handle->diversity_enable=0;
        buffer_hash&=0xffff;
        serverLog(LL_DEBUG,"[data_diversity] new diversity hash has been created as 0x%"PRIx64,buffer_hash);
    }
    handle->last_hash = buffer_hash;  
    handle->output_index++;
    serverLog(LL_DEBUG,"[write_hook] output_index: %"PRIu64", buffer_size: %zu, buffer_hash: 0x%"PRIx64", buff: %s",handle->output_index,buffer_size, buffer_hash,tmp_buf);
    serverLog(LL_DEBUG,"[Call_API] (index, hash) = (%"PRIu64",0x%"PRIx64")",handle->output_index,buffer_hash);
    report_hashval(handle->output_index,buffer_hash);
    // add into hash_list
    OutputHashNode* node = malloc(sizeof(OutputHashNode));
    node->index = handle->output_index;
    node->hashval = buffer_hash; 
    listAddNodeTail(handle->hash_list, node); 
    // inc index
    free(tmp_buf);
    return 0;
}

