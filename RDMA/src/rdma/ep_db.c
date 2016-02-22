#include <rdma_ep_db.h>

ep_t* ep_search(struct rb_root *root, const uint16_t lid )
{
    struct rb_node *node = root->rb_node;

    while (node) 
    {
        ep_t *ep = container_of(node, ep_t, node);

        if (lid < ep->ud_ep.lid)
            node = node->rb_left;
        else if (lid > ep->ud_ep.lid)
            node = node->rb_right;
        else
            return ep;
    }
    return NULL;
}

ep_t* ep_insert( struct rb_root *root, const uint16_t lid )
{
    ep_t *ep;
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    
    while (*new) 
    {
        ep_t *this = container_of(*new, ep_t, node);
        
        parent = *new;
        if (lid < this->ud_ep.lid)
            new = &((*new)->rb_left);
        else if (lid > this->ud_ep.lid)
            new = &((*new)->rb_right);
        else
            return NULL;
    }
    
    /* Create new rr */
    ep = (ep_t*)malloc(sizeof(ep_t));
    ep->ud_ep.lid = lid;
    ep->last_req_id = 0;

    /* Add new node and rebalance tree. */
    rb_link_node(&ep->node, parent, new);
    rb_insert_color(&ep->node, root);

    return ep;
}

void ep_dp_reply_read_req(struct rb_root *root, uint64_t idx)
{
    int rc;
    struct rb_node *node;
    ep_t *ep;
    
    for (node = rb_first(root); node; node = rb_next(node)) 
    {
        ep = rb_entry(node, ep_t, node);
        if (!ep->wait_for_idx) continue;

        if (ep->wait_for_idx < idx) {
            //ud_clt_answer_read_request(ep);
            do_action_to_server();
        }
    }
}