#include "../include/shm/shm.h"

shm_data shared_memory;

void init_shm(node_id_t node_id, int size)
{
	key_t key[size];
	memset(key, 0, sizeof(key_t)*size);
 	key[0] = 5677;
	key[1] = 5678;
	key[2] = 5679;
	int shmid[size];
	//create the segment
	shmid[node_id] = shmget(key[node_id], LOG_SIZE, IPC_CREAT | 0666);
        //printf("just a word\n");
	printf("node_id %d, create new shm key %d, shmid %d\n",node_id, key[node_id],shmid[node_id]);
	//now we attach the segment to our data space
	shared_memory.shm[node_id] = (log_entry*)shmat(shmid[node_id], NULL, 0);
	//shared_memory.shm[node_id] = shared_memory.shm_log->entries;
	//shared_memory.shm_log->len  = LOG_SIZE;
	//shared_memory.shm_log->end  = 0;
	//shared_memory.shm_log->tail = 0;

loop:
	for (int i = 0; i < size; ++i)
	{
		if (i == node_id)
			continue;
		int id = shmget(key[i], LOG_SIZE, 0666);
		//printf("key %d, id %d, errno is %d, errmsg is %s\n",key[i],id,errno,strerror(errno));
		if (-1 == id) {
			goto loop;
		}else{
			printf("connect to %d node successfully.\n",key[i]);
			shmid[i]=id;
			shared_memory.shm[i] = (log_entry*)shmat(shmid[i], NULL, 0);
		}
	}
	printf("init shm finished\n");

}
