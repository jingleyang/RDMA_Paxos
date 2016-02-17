#include "../include/shm/shm.h"
#include "../include/rsm-interface.h"

shm_data shared_memory;

void init_shm(node_id_t node_id, int size)
{
	key_t key[size] = {5677, 5678, 5679};
	int shmid[size];
	//create the segment
	shmid[node_id] = shmget(key[node_id], sizeof(log) + LOG_SIZE, IPC_CREAT | 0666);
	//now we attach the segment to our data space
	shared_memory.shm_log = (log*)shmat(shmid[node_id], NULL, 0);
	shared_memory.shm[node_id] = shared_memory.shm_log->entries;
	shared_memory.shm_log.len  = LOG_SIZE;
    shared_memory.shm_log.end  = 0;
    shared_memory.shm_log.tail = 0;

loop:
	for (int i = 0; i < size; ++i)
	{
		if (i == node_id)
			continue;
		if ((shmid[i] = shmget(key[i], sizeof(log) + LOG_SIZE, 0666)) < 0) {
			goto loop;
    	}else{
    		shared_memory.shm[i] = (log_entry*)shmat(shmid[i], NULL, 0);
    	}
	}

}