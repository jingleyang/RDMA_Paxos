ib_device_t *ib_device;
#define IBDEV ib_device
#define SRV_DATA ((server_data_t*)ib_device->udata)

int init_ib_device()
{
    int i;
    int num_devs;
    struct ibv_device **ib_devs = NULL;
    
    /* Get list of devices (HCAs) */ 
    ib_devs = ibv_get_device_list(&num_devs);
    
    /* Go through all devices and find one that we can use */
    for (i = 0; i < num_devs; i++) {
        /* Init device */
        IBDEV = init_one_device(ib_devs[i]);
        if (NULL != IBDEV) {
            /* Found it */
            break;
        }
    }
    
    /* Free device list */
    ibv_free_device_list(ib_devs);
    
    if (NULL == IBDEV) {
        /* Cannot find device */
        return 1;
    }

    /* Initialize IB UD connection */
    ud_init();

    return 0; 
}

/**
 * Post UD receives
 * Note: After this call, we may receive messages over UD
 */
int start_ib_ud()
{
    return ud_start();
}

int init_ib_srv_data(void *data)
{
    int i;
    IBDEV->udata = data;
     
    for (i = 0; i < SRV_DATA->config.len; i++) {
        SRV_DATA->config.servers[i].ep = malloc(sizeof(ib_ep_t));
        memset(SRV_DATA->config.servers[i].ep, 0, sizeof(ib_ep_t));
    }
    
    return 0;
}

/** 
 * Init IB reliable connection (RC)
 */
int init_ib_rc()
{
    return rc_init();
}

ib_device_t* init_one_device(struct ibv_device* ib_dev)
{
    int i;
    ib_device_t *device = NULL;
    struct ibv_context *dev_context = NULL;
    
    /* Open up the device */
    dev_context = ibv_open_device(ib_dev);
    if (NULL == dev_context) {
        goto error;
    }
    
    /* Allocate new device */
    device = (ib_device_t*)malloc(sizeof(ib_device_t));
    if (NULL == device) {
        goto error;
    }
    memset(device, 0, sizeof(ib_device_t));
    
    /* Init device */
    device->ib_dev = ib_dev;
    device->ib_dev_context = dev_context;
    
    /* Get device's attributes */
    if(ibv_query_device(device->ib_dev_context, &device->ib_dev_attr)){
        goto error;
    }

    /* Find port */
    device->port_num = 0;
    for (i = 1; i <= device->ib_dev_attr.phys_port_cnt; i++) {
        struct ibv_port_attr ib_port_attr;
        if (ibv_query_port(device->ib_dev_context, i, &ib_port_attr)) {
            goto error;
        }
        if (IBV_PORT_ACTIVE != ib_port_attr.state) {
            continue;
        }

        /* find index of pkey 0xFFFF */
        uint16_t pkey, j;
        for (j = 0; j < device->ib_dev_attr.max_pkeys; j++) {
            if (ibv_query_pkey(device->ib_dev_context, i, j, &pkey)) {
                goto error;
            }
            pkey = ntohs(pkey);// & IB_PKEY_MASK;
            if (pkey == 0xFFFF) {
                device->pkey_index = j;
                break;
            }
        }
        device->mtu = IBV_MTU_4096;

        device->port_num = i;
        
        break;
    }
    if (0 == device->port_num) {
        goto error;
    }
      
    return device;

error:
    /* Free device */
    if (NULL != device) {
        free(device);
    }
    /* Free the device context */
    if (NULL != dev_context) {
        ibv_close_device(dev_context);
    }
    return NULL; 
}

/* ================================================================== */
/* Starting a server */

uint8_t ib_poll_ud_queue()
{
    return ud_get_message();
}

int ib_join_cluster()
{
    return ud_join_cluster();
}

int ib_exchange_rc_info()
{
    return ud_exchange_rc_info();
}