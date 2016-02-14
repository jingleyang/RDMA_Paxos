shm_data_t *shm_data;
#define SHM_DATA shm_data

init_shm(node_id_t node_id, int size)
{
	key_t key[size] = {5677, 5678, 5679};
	int shmid[size];
	shmid[node_id] = shmget(key[node_id], sizeof(log_t) + LOG_SIZE, IPC_CREAT | 0666);
	SHM_DATA->shm[node_id] = (log_entry_t*)shmat(shmid[node_id], NULL, 0);
	SHM_DATA->log->len  = LOG_SIZE;
    SHM_DATA->log->end  = SHM_DATA->log->len;
    SHM_DATA->log->tail = SHM_DATA->log->len;

loop:
	for (int i = 0; i < size; ++i)
	{
		if (i == node_id)
			continue;
		if ((shmid[i] = shmget(key[i], sizeof(log_t) + LOG_SIZE, 0666)) < 0) {
			goto loop;
    	}else{
    		SHM_DATA->shm[i] = (log_entry_t*)shmat(shmid[i], NULL, 0);
    	}
	}

}