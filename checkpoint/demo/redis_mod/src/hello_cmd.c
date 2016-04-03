/*
Date: 17 March, 2016
Author: Jingyu Yang
Description: A demo to show how to add a command for redis-server,
And I add some code about hash value proposal.
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

// I add a line to test pull request.^^
// I add a line to test branch pull request. haha

#define TOTAL_NODE_SIZE 3
list* roundFilter(list* listHead, uint64_t round){ // The function will return a list of hashvalues of a particular round number.
    listIter li;
    listNode *ln;
    list* roundList = listCreate(); 
    listRewind(listHead,&li);
    while((ln = listNext(&li))){
        OutputHashNode* node = listNodeValue(ln);
        if (node->index == round){
            listAddNodeTail(roundList,node);
        } 
    }
    return roundList; 
}
int count_hash(uint64_t aim_hash, list* listHead){
    listIter li;
    listNode *ln;
    listRewind(listHead,&li);
    int cnt=0;
    while((ln = listNext(&li))){   
        OutputHashNode* node = listNodeValue(ln);
        if (node->hashval == aim_hash){  
            cnt++;
        }                           
    }   
    return cnt;
}

int major_count_hash(list* listHead){
    listIter li;
    listNode *ln;  
    listRewind(listHead,&li);
    int max_cnt = 0;
    while ((ln=listNext(&li))){
        OutputHashNode* node = listNodeValue(ln);
        int ret = count_hash(node->hashval,listHead);
        if (ret>max_cnt){ // update max of cnt
            max_cnt = ret;  
        }
    }
    return max_cnt;
}
 
int hash_consensus(uint64_t master_index,uint64_t master_hash, list* roundList){
    serverLog(LL_DEBUG,"[Hash_decision] m_index: %"PRIu64" m_hash: 0x%"PRIx64,master_index,master_hash); 
    int cnt = count_hash(master_hash,roundList);  
    int con_num = cnt+1; // include master itself
    int threshold = TOTAL_NODE_SIZE/2 +1; 
    if (con_num == TOTAL_NODE_SIZE){ // D.0 all hash are the same.
        serverLog(LL_DEBUG,"[hash_consensus] D.0 All hash are the same (Nothing to do)");        return 0;     
    }
    if (con_num >= threshold ){ // D.1 H(header) == H(major). 
        serverLog(LL_DEBUG,"[hash_consensus] D.1 Minority need redo."); 
        return 1;
    }
    int major_cnt = major_count_hash(roundList); 
    if (major_cnt >= threshold){ // D2. H(header) != H(major).
        serverLog(LL_DEBUG,"[hash_consensus] D.2 Master and Minority need redo."); 
        return 2;   
    }
    // consensus failed
    serverLog(LL_DEBUG,"[hash_consensus] D.3 All nodes need redo.");
    return 3; 
}
void check_quorum(){
    Singleton_OutputHash* handle = get_OutputHash_instance();
    list* roundList = roundFilter(handle->hash_collect,handle->output_index);
    int len = listLength(roundList);
    if ((TOTAL_NODE_SIZE-1) == len){ // collected enough info
        // if (handle->output_index%2000 == 0){ // Do hash consensus every 2000 times.
        //       hash_consensus();      
        // }
        //}
        //
        int decision = hash_consensus(handle->output_index, handle->last_hash,roundList);        serverLog(LL_DEBUG,"[check_quorum] Got a decision: %d",decision);
    }   
    // delete roundlist    
    listRelease(roundList);
}
void collect_hashval(OutputHashNode* node){
    Singleton_OutputHash* handle = get_OutputHash_instance();
    listAddNodeTail(handle->hash_collect,node);            
}
void report_hashCommand(client *c){
    //serverLog(LL_DEBUG,"[report_hash] argc=%d, argv[0]=%s, argv[1]=%s, argv[2]=%s",c->argc,(char*)c->argv[0]->ptr, (char*)c->argv[1]->ptr,(char*)c->argv[2]->ptr);
    uint64_t index = strtoull((char*)c->argv[1]->ptr,NULL,10);
    uint64_t hashval = strtoull((char*)c->argv[2]->ptr,NULL,10);
    const char* slaveName = (const char*)c->argv[3]->ptr;     
    OutputHashNode* node = malloc(sizeof(OutputHashNode));
    node->index = index;
    node->hashval = hashval;
    strncpy(node->owner,slaveName,BUFF_SIZE-1);

    serverLog(LL_DEBUG,"[report_hash] recvied index:%"PRIu64", hashval:0x%"PRIx64" from %s",index,hashval,slaveName);
    Singleton_OutputHash* handle = get_OutputHash_instance(); 
    collect_hashval(node);
    check_quorum();
    char output_buff[BUFF_SIZE];
    snprintf(output_buff,BUFF_SIZE-1,"M_hash %"PRIu64" %"PRIu64" master",handle->output_index,handle->last_hash);
    addReplyBulkCString(c,output_buff);
}
void diversityCommand(client *c){
    char* hash_ptr = c->argv[1]->ptr;
    uint64_t diversity_hash = strtoull(hash_ptr,NULL,16); 
    serverLog(LL_DEBUG,"[data_diversity] will create a diversity hash 0x%"PRIx64" at the next request",diversity_hash); 
    Singleton_OutputHash* handle = get_OutputHash_instance();    
    handle->diversity_enable=1;
    handle->diversity_hash = diversity_hash;
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
        instance->diversity_hash = 0;
        instance->output_index = -1; // because I will do inc firstly
        instance->last_hash = 0;
        instance->hash_list = listCreate(); 
        instance->hash_collect = listCreate();
    }else{
        //serverLog(LL_DEBUG,"[singleton] handle has been reused");
    }
    return instance;
}
void report_to_master(const char* hostname, int port, uint64_t index, uint64_t hashval, const char* slaveName){
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
    reply = redisCommand(c,"report_hash %"PRIu64" %"PRIu64" %s",index,hashval,slaveName); 
    serverLog(LL_DEBUG,"[report_hash] reply: %s",reply->str);
    freeReplyObject(reply);
    redisFree(c);
}
void report_hashval(uint64_t index, uint64_t hashval){
    serverLog(LL_DEBUG,"[report_hash] masterhost:%s, masterport:%d",server.masterhost,server.masterport);
    if (NULL == server.masterhost){
        serverLog(LL_DEBUG,"[report_hash] I am the master. I will not report hashval");
        return;
    }else{
        const char* slaveName=NULL;
        if (16379==server.port){ // self port
            slaveName="Slave01";
        }else{
            slaveName="Slave02";
        }
        serverLog(LL_DEBUG,"[report_hash] I am a slave. I will report to the master");
        report_to_master(server.masterhost,server.masterport,index,hashval,slaveName); 
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
    uint64_t buffer_hash=crc64(handle->last_hash,(const unsigned char*)buffer_ptr,buffer_size); // Please include crc64.h/c if you wants to use crc64().
    if (handle->diversity_enable){
        handle->diversity_enable=0;
        buffer_hash=handle->diversity_hash;
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
    strncpy(node->owner,"Master",BUFF_SIZE-1); 
    listAddNodeTail(handle->hash_list, node); 
    // inc index
    free(tmp_buf);
    return 0;
}

