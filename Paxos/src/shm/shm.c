#include "../include/shm/shm.h"
#include "../include/log/log.h"

struct shm_data_t {
	log  *lg;
	log_entry* shm[MAX_SERVER_COUNT];
};

shm_data *shm;
#define SHM_DATA shm

void init_shm(node_id_t node_id, int size)
{
	key_t key[size] = {5677, 5678, 5679};
	int shmid[size];
	shmid[node_id] = shmget(key[node_id], sizeof(log) + LOG_SIZE, IPC_CREAT | 0666);
	SHM_DATA->shm[node_id] = (log_entry*)shmat(shmid[node_id], NULL, 0);
	SHM_DATA->lg->len  = LOG_SIZE;
    SHM_DATA->lg->end  = SHM_DATA->lg->len;
    SHM_DATA->lg->tail = SHM_DATA->lg->len;

loop:
	for (int i = 0; i < size; ++i)
	{
		if (i == node_id)
			continue;
		if ((shmid[i] = shmget(key[i], sizeof(log) + LOG_SIZE, 0666)) < 0) {
			goto loop;
    	}else{
    		SHM_DATA->shm[i] = (log_entry*)shmat(shmid[i], NULL, 0);
    	}
	}

}