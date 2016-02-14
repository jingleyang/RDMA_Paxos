struct shm_data_t {
	log_t  *log;
	log_entry_t* shm[group_size];
};
typedef struct shm_data_t shm_data_t;