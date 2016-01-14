int store_record(db* db_p, size_t key_size, void* key_data, size_t data_size, void* data){
    int ret = 1;
    DB* b_db = db_p->bdb_ptr;
    DBT key, db_data;
    memset(&key, 0, sizeof(key));
    memset(&db_data ,0, sizeof(db_data));
    key.data = key_data;
    key.size = key_size;
    db_data.data = data;
    db_data.size = data_size;
    if ((ret = b_db->put(b_db,NULL,&key,&db_data,DB_AUTO_COMMIT)) == 0){
    }
    return ret;
}