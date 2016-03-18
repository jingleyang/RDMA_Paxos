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
	int rc, i, zoo_data_len = ZDATALEN;
	char str[512];
	
	sprintf(str, "%"PRId64",%"PRIu32"", consensus_comp->node_id, srv_data.tail);
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

	char *p;
	struct Stat *stat;
	znodes_data *znodes = (znodes_data*)malloc(sizeof(znodes_data) * MAX_SERVER_COUNT);
	for (i = 0; i < children_list->count; ++i)
	{
		char *zoo_data = malloc(ZDATALEN * sizeof(char));
		char temp_znode_name[512];
		get_znode_name(children_list->data[i], temp_znode_name);

		rc = zoo_get(zh, temp_znode_name, 0, zoo_data, &zoo_data_len, stat);
		if (rc)
		{
			fprintf(stderr, "Error %d for zoo_get\n", rc);
		}
		p = strtok(zoo_data, ",");
		znodes[i]->node_id = atoi(p);
		p = strtok(NULL, ",");
		znodes[i]->tail = atoi(p);
		znodes[i]->c_time = stat->c_time;
		free(zoo_data);
	}
	uint32_t max_tail;
	for (i = 0; i < children_list->count - 1; i++)
	{
		if (znodes[i]->tail > znodes[i + 1]->tail) {	
			max_tail = znodes[i]->tail;
		}else{
			max_tail = znodes[i + 1]->tail;
		}
	}

	uint32_t max_c_time;
	for (i = 0; i < children_list->count - 1; i++)
	{
		if (znodes[i]->c_time > znodes[i + 1]->c_time) {	
			max_c_time = znodes[i]->c_time;
		}else{
			max_c_time = znodes[i + 1]->c_time;
		}
	}

	for (i = 0; i < children_list->count; ++i)
	{
		if (znodes[i]->c_time == max_c_time && znodes[i]->tail == max_tail)
		{
			consensus_comp->cur_view.leader_id = znodes[i]->node_id;
			break;
		}
	}

	if (consensus_comp->cur_view.leader_id == consensus_comp->node_id)
	{
		consensus_comp->my_role = LEADER;
		//fprintf(stderr, "I am the leader\n");
		for (i = 0; i < children_list->count - 1; ++i)
		{
			if (znodes[i]->tail != znodes[i + 1]->tail)
			{
				//recheck
			}
		}
	}else{
		consensus_comp->my_role = SECONDARY;
		//fprintf(stderr, "I am a follower\n");
		// RDMA read
		// update view
		// zoo_set
	}
	free(znodes);
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
	consensus_comp->znode_name = (char*)malloc(strlen(znode_name));
	strcpy(consensus_comp->znode_name, znode_name);
	check_leader(consensus_comp);

    rc = zoo_wget_children(zh, "/election", zoo_wget_children_watcher, (void*)consensus_comp, NULL);
    if (rc)
    {
    	fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
    }
	return 0;
}