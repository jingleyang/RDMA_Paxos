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
	//now we attach the segment to our data space
	shared_memory.shm[node_id] = shmat(shmid[node_id], NULL, 0);
	shared_memory.tail = 0;

loop:
	for (int i = 0; i < size; ++i)
	{
		if (i == node_id)
			continue;
		int id = shmget(key[i], LOG_SIZE, 0666);
		if (-1 == id) {
			goto loop;
		}else{
			shmid[i]=id;
			shared_memory.shm[i] = shmat(shmid[i], NULL, 0);
		}
	}

}
