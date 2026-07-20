#include "myjql.h"

#include "buffer_pool.h"
#include "b_tree.h"
#include "table.h"
#include "str.h"

#include<stdlib.h>

typedef struct {
    RID key;
    RID value;
} Record;

void read_record(Table *table, RID rid, Record *record) {
    table_read(table, rid, (ItemPtr)record);
}

RID write_record(Table *table, const Record *record) {
    return table_insert(table, (ItemPtr)record, sizeof(Record));
}

void delete_record(Table *table, RID rid) {
    table_delete(table, rid);
}

BufferPool bp_idx;
Table tbl_rec;
Table tbl_str;

void myjql_init() {
    b_tree_init("rec.idx", &bp_idx);
    table_init(&tbl_rec, "rec.data", "rec.fsm");
    table_init(&tbl_str, "str.data", "str.fsm");
}

void myjql_close() {
    /* validate_buffer_pool(&bp_idx);
    validate_buffer_pool(&tbl_rec.data_pool);
    validate_buffer_pool(&tbl_rec.fsm_pool);
    validate_buffer_pool(&tbl_str.data_pool);
    validate_buffer_pool(&tbl_str.fsm_pool); */
    b_tree_close(&bp_idx);
    table_close(&tbl_rec);
    table_close(&tbl_str);
}

int compareptr(char* key, size_t size, RID rid) {
    Record* record = (Record*)malloc(sizeof(Record));
    StringRecord* stringrecord = (StringRecord*)malloc(sizeof(StringRecord));
    if(get_rid_block_addr(rid) == -1 && get_rid_idx(rid) == 0) 
    {
        return;
    }
    read_record(&tbl_rec, rid, record);
    read_string(&tbl_str, record->key, stringrecord);
    StringRecord stringrecordvalue = *stringrecord;
    stringrecordvalue.idx = 0;
    char* copmarestring = get_str_chunk_data_ptr(&stringrecordvalue.chunk);
    if(key[0] > copmarestring[0]) 
    {
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        return 1;
    }
    else if(key[0] < copmarestring[0]) 
    {
        if(record)
        {
            free(record);
        } 
        if(stringrecord)
        {
            free(stringrecord);
        }
        return -1;
    }
    size_t midsize = 1;
    while(midsize < size) 
    {
        if(has_next_char(&stringrecordvalue) == 1 && key[midsize] != 0) 
        {
            copmarestring[0] = next_char(&tbl_str, &stringrecordvalue);
            if(key[midsize] > copmarestring[0]) 
            {
                if(record)
                {
                    free(record);
                }
                if(stringrecord)
                {
                    free(stringrecord);
                }
                return 1;
            }
            else if(key[midsize] < copmarestring[0]) 
            {
                if(record)
                {
                    free(record);
                }
                if(stringrecord)
                {
                    free(stringrecord);
                }
                return -1;
            }
        }
        else if(key[midsize] == 0 || has_next_char(&stringrecordvalue) == 0) 
        {
            if(key[midsize] != 0 && midsize != size - 1) 
            {
                if(record)
                {
                    free(record);
                }
                if(stringrecord)
                {
                    free(stringrecord);
                }
                return 1;
            }
        }
        midsize++;
    }
    if(has_next_char(&stringrecordvalue)) 
    {
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        return -1;
    }
    else 
    {
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        return 0;
    }
}

size_t myjql_get(const char *key, size_t key_len, char *value, size_t max_size) {
    Record* record = (Record*)malloc(sizeof(Record));
    StringRecord* stringrecord = (StringRecord*)malloc(sizeof(StringRecord));
    size_t size;
    RID recordrid = b_tree_search(&bp_idx, key, key_len, compareptr);
    if(get_rid_block_addr(recordrid) == -1 && get_rid_idx(recordrid) == 0) 
    {
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        return -1;
    }
    read_record(&tbl_rec, recordrid, record);
    read_string(&tbl_str, record->value, stringrecord);
    size = load_string(&tbl_str, stringrecord, value, max_size);
    if(record)
    {
        free(record);
    }
    if(stringrecord)
    {
        free(stringrecord);
    }  
    return size;
}

int comparerow(RID rid1, RID rid2) 
{
    Record* record1 = (Record*)malloc(sizeof(Record));
    Record* record2 = (Record*)malloc(sizeof(Record));
    StringRecord* recordkey1 = (StringRecord*)malloc(sizeof(StringRecord));
    StringRecord* recordkey2 = (StringRecord*)malloc(sizeof(StringRecord));  
    if(get_rid_block_addr(rid1) == -1 && get_rid_idx(rid1) == 0)
    {
        return;
    }
    if(get_rid_block_addr(rid2) == -1 && get_rid_idx(rid2) == 0)
    {
        return;
    }
    read_record(&tbl_rec, rid1, record1);
    read_record(&tbl_rec, rid2, record2);
    int result = 2;
    if(record1 != NULL && record2 != NULL)
    {
        read_string(&tbl_str, record1->key, recordkey1);
        read_string(&tbl_str, record2->key, recordkey2);
        result = compare_string_record(&tbl_str, recordkey1, recordkey2);
    }
    if(record1)
    {
        free(record1);
    }
    if(record2)
    {
        free(record2);
    }
    if(recordkey1)
    {
        free(recordkey1);
    }
    if(recordkey2)
    {
        free(recordkey2);
    }
    if(result != 2)
    {
        return result;
    }
    else 
    {
        return;
    }
}

RID insert_handler(RID rid) {}
void delete_handler(RID rid) {}

void myjql_set(const char *key, size_t key_len, const char *value, size_t value_len) {
    Record* record = (Record*)malloc(sizeof(Record));
    StringRecord* stringrecord = (StringRecord*)malloc(sizeof(StringRecord));
    RID recordrid = b_tree_search(&bp_idx, key, key_len, compareptr);
    if(get_rid_block_addr(recordrid) == -1 && get_rid_idx(recordrid) == 0) 
    {  
        RID keyrid = write_string(&tbl_str, key, key_len);
        RID valuerid = write_string(&tbl_str, value, value_len);
        record->key = keyrid;
        record->value = valuerid;
        recordrid = write_record(&tbl_rec, record);
        b_tree_insert(&bp_idx, recordrid, comparerow, insert_handler);
        if(record)
        {
            free(record);
        }   
        if(stringrecord)
        {
            free(stringrecord);
        }
    }
    else 
    {
        b_tree_delete(&bp_idx, recordrid, comparerow, insert_handler, delete_handler);
        read_record(&tbl_rec, recordrid, record);
        delete_string(&tbl_str, record->key);
        delete_string(&tbl_str, record->value);
        delete_record(&tbl_rec, recordrid);
        RID keyrid = write_string(&tbl_str, key, key_len);
        RID valuerid = write_string(&tbl_str, value, value_len);
        Record* newrecord = (Record*)malloc(sizeof(Record));
        newrecord->key = keyrid;
        newrecord->value = valuerid;
        RID newrec_rid = write_record(&tbl_rec, newrecord);
        b_tree_insert(&bp_idx, newrec_rid, comparerow, insert_handler);
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        if(newrecord)
        {
            free(newrecord);
        }
    }
}

void myjql_del(const char *key, size_t key_len) {
    Record* record = (Record*)malloc(sizeof(Record));
    StringRecord* stringrecord = (StringRecord*)malloc(sizeof(StringRecord));
    RID recordrid = b_tree_search(&bp_idx, key, key_len, compareptr);
    if(get_rid_block_addr(recordrid) == -1 && get_rid_idx(recordrid) == 0) 
    {
        if(record)
        {
            free(record);
        }
        if(stringrecord)
        {
            free(stringrecord);
        }
        return;
    }
    b_tree_delete(&bp_idx, recordrid, comparerow, insert_handler, delete_handler);
    read_record(&tbl_rec, recordrid, record);
    delete_string(&tbl_str, record->key);
    delete_string(&tbl_str, record->value);
    delete_record(&tbl_rec, recordrid);
    if(record)
    {
        free(record);
    }
    if(stringrecord)
    {
        free(stringrecord);
    }
}

/* void myjql_analyze() {
    printf("Record Table:\n");
    analyze_table(&tbl_rec);
    printf("String Table:\n");
    analyze_table(&tbl_str);
} */