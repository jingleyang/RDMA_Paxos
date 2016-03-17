#include "../include/zookeeper/zoo.h"
#include "../include/rdma/rdma_common.h"
#include <zookeeper.h>

dare_server_data_t srv_data;

static zhandle_t *zh;
static int is_connected;

#define ZDATALEN 1024 * 1024

static void get_znode_name(const char *pathbuf, char *znode_name)
{
	const char *p = pathbuf;
	int i;
	for (i = strlen(pathbuf); i >= 0; i--)
	{
		if (*(p + i) == '/')
		{
			break;
		}
	}
	strcpy(znode_name, "/election/");
	strcat(znode_name, p + i + 1);
}

void zookeeper_init_watcher(zhandle_t *izh, int type, int state, const char *path, void *context)
{
	if (type == ZOO_SESSION_EVENT)
	{
		if (state == ZOO_CONNECTED_STATE)
		{
			is_connected = 1;
		} else if (state == ZOO_EXPIRED_SESSION_STATE) {
			is_connected = 0;
			zookeeper_close(izh);
		}
	}
}

static int check_leader(consensus_component* consensus_comp)
{
	int rc;
	char str[512];
	
	sprintf(str, "%"PRId64",%"PRIu32",%"PRIu32"", consensus_comp->node_id, srv_data.tail, consensus_comp->cur_view.view_id);
	rc = zoo_set(zh, consensus_comp->znode_name, str, strlen(str), -1);
	if (rc)
	{
		fprintf(stderr, "Error %d for zoo_set\n", rc);
	}
	struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
	rc = zoo_get_children(zh, "/election", 0, children_list);
	if (rc)
	{
		fprintf(stderr, "Error %d for zoo_get_children\n", rc);
	}

	int zoo_data_len = ZDATALEN;
	char *p;
	node_id_t temp_node_id[MAX_SERVER_COUNT];
	for (int i = 0; i < children_list->count; ++i)
	{
		char *zoo_data = malloc(ZDATALEN * sizeof(char));
		char temp_znode_name[512];
		get_znode_name(children_list->data[i], temp_znode_name);

		rc = zoo_get(zh, temp_znode_name, 0, zoo_data, &zoo_data_len, NULL);
		if (rc)
		{
			fprintf(stderr, "Error %d for zoo_get\n", rc);
		}
		p = strtok(zoo_data, ",");
		temp_node_id[i] = atoi(p);
		p = strtok(NULL, ",");
		consensus_comp->peer_pool[temp_node_id[i]].tail = atoi(p);
		free(zoo_data);
	}
	uint32_t max_tail;
	for (int i = 0; i < children_list->count - 1; i++)
	{
		if (consensus_comp->peer_pool[temp_node_id[i]].tail > consensus_comp->peer_pool[temp_node_id[i + 1]].tail) {	
			max_tail = consensus_comp->peer_pool[temp_node_id[i]].tail;
		}else{
			max_tail = consensus_comp->peer_pool[temp_node_id[i + 1]].tail;
		}
	}

	node_id_t leader_id;
	int flag;
	for (leader_id = 0; leader_id < consensus_comp->group_size; leader_id++)
	{
		flag = 0;
		for (int i = 0; i < children_list->count; ++i)
		{
			if (leader_id == temp_node_id[i])
			{
				flag = 1;
				break;
			}
		}
		if (!flag)
			continue;
		if (consensus_comp->peer_pool[leader_id].tail == max_tail){
			break;
		}
	}

	consensus_comp->cur_view.leader_id = leader_id;

	if (leader_id == consensus_comp->node_id)
	{
		consensus_comp->my_role = LEADER;
		// recheck
	}else{
		consensus_comp->my_role = SECONDARY;
		// RDMA read
		// update view
		// zoo_set
	}

	return 0;
}

void zoo_wget_children_watcher(zhandle_t *wzh, int type, int state, const char *path, void *watcherCtx) {
	if (type == ZOO_CHILD_EVENT)
	{
		/* a change as occurred in the list of children. */
		int rc;
		consensus_component* consensus_comp = (consensus_component*)watcherCtx;
		// block the threads
		rc = zoo_wget_children(wzh, "/election", zoo_wget_children_watcher, watcherCtx, NULL);
		if (rc)
		{
			fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
		}
		check_leader(consensus_comp);	
	}
}

int init_zookeeper(consensus_component* consensus_comp)
{
	srv_data.tail = 0;
	int rc;
	char path_buffer[512];
	zh = zookeeper_init(consensus_comp->zoo_host_port, zookeeper_init_watcher, 15000, 0, 0, 0);

	while(is_connected != 1);
	int interest, fd;
	struct timeval tv;
	zookeeper_interest(zh, &fd, &interest, &tv);
	consensus_comp->zfd = fd;

	rc = zoo_create(zh, "/election/guid-n_", NULL, 0, &ZOO_OPEN_ACL_UNSAFE, ZOO_SEQUENCE|ZOO_EPHEMERAL, path_buffer, 512);
	if (rc)
	{
		fprintf(stderr, "Error %d for zoo_create\n", rc);
	}

	char znode_name[512];
	get_znode_name(path_buffer, znode_name);
	consensus_comp->znode_name = znode_name;
	check_leader(consensus_comp);

    rc = zoo_wget_children(zh, "/election", zoo_wget_children_watcher, (void*)consensus_comp, NULL);
    if (rc)
    {
    	fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
    }
	return 0;
}