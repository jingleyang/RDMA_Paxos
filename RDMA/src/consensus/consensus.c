#define _GNU_SOURCE

#include "../include/consensus/consensus.h"
#include "../include/consensus/consensus-msg.h"

#include "../include/rdma/dare_ibv_rc.h"
#include "../include/rdma/dare_server.h"

#include <dlfcn.h>

#define IBDEV dare_ib_device
#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

consensus_component* init_consensus_comp(struct node_t* node,struct sockaddr_in my_address,pthread_mutex_t* lock,uint32_t node_id,FILE* log,int sys_log,int stat_log,const char* db_name,void* db_ptr,int group_size,
    view* cur_view,view_stamp* to_commit,view_stamp* highest_committed_vs,view_stamp* highest,void* arg){
    consensus_component* comp = (consensus_component*)malloc(sizeof(consensus_component));
    memset(comp,0,sizeof(consensus_component));

    if(NULL!=comp){
        comp->db_ptr = db_ptr;  
        comp->sys_log = sys_log;
        comp->stat_log = stat_log;
        comp->sys_log_file = log;
        comp->my_node = node;
        comp->node_id = node_id;
        comp->group_size = group_size;
        comp->cur_view = cur_view;
        if(comp->cur_view->leader_id == comp->node_id){
            comp->my_role = LEADER;
        }else{
            comp->my_role = SECONDARY;
        }
        comp->highest_seen_vs = highest;
        comp->highest_seen_vs->view_id = 1;
        comp->highest_seen_vs->req_id = 0;
        comp->highest_committed_vs = highest_committed_vs; 
        comp->highest_committed_vs->view_id = 1; 
        comp->highest_committed_vs->req_id = 0; 
        comp->highest_to_commit_vs = to_commit;
        comp->highest_to_commit_vs->view_id = 1;
        comp->highest_to_commit_vs->req_id = 0;
        comp->my_address = my_address;
        comp->lock = lock;

        comp->output_handler = init_output(cur_view);

        goto consensus_init_exit;

    }
consensus_init_exit:
    return comp;
}

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs->view_id;
    next_vs.req_id = (comp->highest_seen_vs->req_id+1);
    return next_vs;
};

static int reached_quorum(uint64_t bit_map, int group_size){
    // this may be compatibility issue 
    if(__builtin_popcountl(bit_map) >= ((group_size/2)+1)){
        return 1;
    }else{
        return 0;
    }
}

int rsm_op(struct consensus_component_t* comp, void* data, ssize_t data_size, uint8_t type)
{
    dare_log_entry_t *entry;
    if (type == CSM)
    {
        pthread_mutex_lock(comp->lock);
        view_stamp next = get_next_view_stamp(comp);

        /* record the data persistently */
        db_key_type record_no = vstol(&next);
        uint64_t bit_map = (1<<comp->node_id);
        if(store_record(comp->db_ptr, sizeof(record_no), &record_no, data_size, data))
        {
            fprintf(stderr, "Can not save record from database.\n");
            goto handle_submit_req_exit;
        }

        comp->highest_seen_vs->req_id = comp->highest_seen_vs->req_id + 1;
        entry = log_append_entry(SRV_DATA->log, data_size, data, &next, comp->node_id, comp->highest_committed_vs, type);

        uint32_t offset = (uint32_t)(offsetof(dare_log_t, entries) + SRV_DATA->log->tail);
        rem_mem_t rm;
        dare_ib_ep_t *ep;

        uint32_t i;
        for (i = 0; i < comp->group_size; i++) {
            ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
            if (i == SRV_DATA->config.idx || 0 == ep->rc_connected)
                continue;

            /* Set remote offset */
            rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
            rm.rkey = ep->rc_ep.rmt_mr.rkey;

            post_send(i, entry, log_entry_len(entry), IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);
        }
        pthread_mutex_unlock(comp->lock);
recheck:
        for (i = 0; i < MAX_SERVER_COUNT; i++) {
            if (entry->ack[i].msg_vs.view_id == next.view_id && entry->ack[i].msg_vs.req_id == next.req_id)
            {
                bit_map = bit_map | (1<<entry->ack[i].node_id);
            }
        }
        if (reached_quorum(bit_map, comp->group_size))
        {
            //TODO: do we need the lock here?
            while (entry->msg_vs.req_id > comp->highest_committed_vs->req_id + 1);
            comp->highest_committed_vs->req_id = comp->highest_committed_vs->req_id + 1;
        }else{
            goto recheck;
        }
    }

    if (type == CHECK)
    {
        pthread_mutex_lock(comp->lock);
        if ((*(long*)data / CHECK_PERIOD) % 2 != 0)
        {
            long output_idx = *(long*)data - CHECK_PERIOD;
            entry = log_append_entry(SRV_DATA->log, sizeof(long), &output_idx, NULL, comp->node_id, NULL, type);
            listNode *node = listIndex(comp->output_handler->output_list, output_idx);
            entry->ack[comp->node_id].hash = *(uint64_t*)listNodeValue(node);
            entry->ack[comp->node_id].node_id = comp->node_id;

            uint32_t offset = (uint32_t)(offsetof(dare_log_t, entries) + SRV_DATA->log->tail);
            rem_mem_t rm;
            dare_ib_ep_t *ep;

            uint32_t i;
            for (i = 0; i < comp->group_size; i++) {
                ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
                if (i == SRV_DATA->config.idx || 0 == ep->rc_connected)
                    continue;

                rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
                rm.rkey = ep->rc_ep.rmt_mr.rkey;

                post_send(i, entry, log_entry_len(entry), IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);
            }
            comp->output_handler->prev_offset = SRV_DATA->log->tail;
        } else {
            entry = log_get_entry(SRV_DATA->log, &comp->output_handler->prev_offset);
            uint32_t i;
            for (i = 0; i < comp->group_size; i++)
            {
                SYS_LOG(comp, "For output idx %ld, node%"PRIu32"'s hash value is %"PRIu64"\n", *(long*)entry->data, entry->ack[i].node_id, entry->ack[i].hash);
                entry->ack[i].flag = CONSISTENT;
            }

            rem_mem_t rm;
            dare_ib_ep_t *ep;

            for (i = 0; i < comp->group_size; i++) {
                ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
                if (i == SRV_DATA->config.idx || 0 == ep->rc_connected)
                    continue;

                rm.raddr = ep->rc_ep.rmt_mr.raddr + comp->output_handler->prev_offset + ACCEPT_ACK_SIZE * i + offsetof(dare_log_t, entries);
                rm.rkey = ep->rc_ep.rmt_mr.rkey;

                post_send(i, &entry->ack[i], ACCEPT_ACK_SIZE, IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);
            }
        }
        pthread_mutex_unlock(comp->lock);
    }
handle_submit_req_exit:
    return 0;
}

void *handle_accept_req(void* arg)
{
    consensus_component* comp = arg;

    db_key_type start;
    db_key_type end;
    db_key_type index;

    size_t data_size;
    void* retrieve_data = NULL;
    
    dare_log_entry_t* entry;

    for (;;)
    {
        while(comp->cur_view->leader_id != comp->node_id)
        {
            entry = log_get_entry(SRV_DATA->log, &SRV_DATA->log->end);
            if (entry->type == CSM)//TODO atmoic opeartion
            {
                if(entry->msg_vs.view_id < comp->cur_view->view_id){
                // TODO
                //goto reloop;
                }
                // if we this message is not from the current leader
                if(entry->msg_vs.view_id == comp->cur_view->view_id && entry->node_id != comp->cur_view->leader_id){
                // TODO
                //goto reloop;
                }

                // update highest seen request
                if(view_stamp_comp(&entry->msg_vs, comp->highest_seen_vs) > 0){
                    *(comp->highest_seen_vs) = entry->msg_vs;
                }

                db_key_type record_no = vstol(&entry->msg_vs);
                // record the data persistently 
                store_record(comp->db_ptr, sizeof(record_no), &record_no, entry->data_size, entry->data);
                SRV_DATA->log->tail = SRV_DATA->log->end;
                SRV_DATA->log->end += log_entry_len(entry);
                uint32_t offset = (uint32_t)(offsetof(dare_log_t, entries) + SRV_DATA->log->tail + ACCEPT_ACK_SIZE * comp->node_id);

                accept_ack* reply = (accept_ack*)((char*)entry + ACCEPT_ACK_SIZE * comp->node_id);
                reply->node_id = comp->node_id;
                reply->msg_vs.view_id = entry->msg_vs.view_id;
                reply->msg_vs.req_id = entry->msg_vs.req_id;

                rem_mem_t rm;
                dare_ib_ep_t *ep = (dare_ib_ep_t*)SRV_DATA->config.servers[entry->node_id].ep;

                rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
                rm.rkey = ep->rc_ep.rmt_mr.rkey;

                post_send(entry->node_id, reply, ACCEPT_ACK_SIZE, IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);

                if(view_stamp_comp(&entry->req_canbe_exed, comp->highest_committed_vs) > 0)
                {
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    connect(sock, (struct sockaddr*)&comp->my_address, sizeof(struct sockaddr_in)); // - mongoose: Broken pipe. Maybe the server closes the socket
                                                                                                    // - Redis: Accepting client connection: accept: Too many open files
                    typedef ssize_t (*orig_write_type)(int, void *, size_t);
                    orig_write_type orig_write;
                    orig_write = (orig_write_type) dlsym(RTLD_NEXT, "write");

                    start = vstol(comp->highest_committed_vs)+1;
                    end = vstol(&entry->req_canbe_exed);
                    for(index = start; index <= end; index++)
                    {
                        retrieve_record(comp->db_ptr, sizeof(index), &index, &data_size, (void**)&retrieve_data);
                        orig_write(sock, retrieve_data, data_size);
                    }
                    *(comp->highest_committed_vs) = entry->req_canbe_exed;
                    //close(sock);
                }
            }
            if (entry->type == CHECK)
            {
                listNode *node = listIndex(comp->output_handler->output_list, *(long*)entry->data); // Return the element at the specified zero-based index where 0 is the head, 1 is the element next to head and so on.

                SRV_DATA->log->tail = SRV_DATA->log->end;
                SRV_DATA->log->end += log_entry_len(entry);
                uint32_t offset = (uint32_t)(offsetof(dare_log_t, entries) + SRV_DATA->log->tail + ACCEPT_ACK_SIZE * comp->node_id);
                
                accept_ack* reply = (accept_ack*)((char*)entry + ACCEPT_ACK_SIZE * comp->node_id);
                reply->node_id = comp->node_id;
                reply->hash = *(uint64_t*)listNodeValue(node);

                rem_mem_t rm;
                dare_ib_ep_t *ep = (dare_ib_ep_t*)SRV_DATA->config.servers[entry->node_id].ep;

                rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
                rm.rkey = ep->rc_ep.rmt_mr.rkey;

                post_send(entry->node_id, reply, ACCEPT_ACK_SIZE, IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, rm);

                if (*(long*)entry->data != 0)
                {
                    entry = log_get_entry(SRV_DATA->log, &comp->output_handler->prev_offset);
                    SYS_LOG(comp, "My output is %s until idx %ld\n", entry->ack[comp->node_id].flag == CONSISTENT ? "CONSISTENT" : "NOTCONSISTENT", *(long*)entry->data);
                }
                comp->output_handler->prev_offset = SRV_DATA->log->tail;
            }
        }
    }
};
