#include <db.h>

uint64_t vstol(uint64_t term, uint64_t idx){
    uint64_t result = idx&0xFFFFFFFFl;
    uint64_t temp = term&0xFFFFFFFFl;
    result += temp<<32;
    return result;
};

int retrieve_record(db* db_p,size_t key_size,void* key_data,size_t* data_size,void** data){
    int ret=1;
    if(NULL == db_p || NULL == db_p->bdb_ptr){
        goto db_retrieve_return;
    }
    DB* b_db = db_p->bdb_ptr;
    DBT key, db_data;
    memset(&key, 0, sizeof(key));
    memset(&db_data, 0, sizeof(db_data));
    key.data = key_data;
    key.size = key_size;
    db_data.flags = DB_DBT_MALLOC;
    if((ret = b_db->get(b_db, NULL, &key, &db_data, 0)) == 0){
    }else{
        goto db_retrieve_return;
    }
    if(!db_data.size){
        goto db_retrieve_return;
    }
    *data = db_data.data;
    *data_size = db_data.size;
db_retrieve_return:
    return ret;
}

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
    if ((ret = b_db->put(b_db, NULL, &key, &db_data, DB_AUTO_COMMIT)) == 0){
    }
    return ret;
}