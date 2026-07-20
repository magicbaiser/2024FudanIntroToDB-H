#include"str.h"

#include "table.h"

#include <stdlib.h>

void read_string(Table *table, RID rid, StringRecord *record) {
    StringChunk* mid = &record->chunk;
    table_read(table, rid, (ItemPtr)mid);
    record->chunk = *mid;
    record->idx = 0;
}

int has_next_char(StringRecord *record) {
    StringChunk* mid = &(*record).chunk;
    short size = get_str_chunk_size(mid) - sizeof(RID) - sizeof(short);
    if((*record).idx != size - 1) 
    {
        return 1;
    }
    else
    {
        RID rid = get_str_chunk_rid(mid);
        if(get_rid_block_addr(rid) == -1)
        {
            return 0;
        }
    }
    return 1;
}

char next_char(Table *table, StringRecord *record) {
    StringChunk* mid = &record->chunk;
    short size = get_str_chunk_size(mid) - sizeof(RID) - sizeof(short);
    char* data = get_str_chunk_data_ptr(mid);
    if(record->idx != size - 1) 
    {
        record->idx++;
        return data[record->idx];
    }
    else 
    {
        record->idx = 0;
        RID next = get_str_chunk_rid(mid);
        off_t nextaddress = get_rid_block_addr(next);
        if(nextaddress == -1)
        {
            return 0;
        }
        Block* block = (Block*)get_page(&table->data_pool, nextaddress);
        release(&table->data_pool, nextaddress);
        short nextidx = get_rid_idx(next);
        StringChunk* nextchunk = (StringChunk*)get_item(block, nextidx);
        record->chunk = *nextchunk;
        char* nextdata = get_str_chunk_data_ptr(nextchunk);
        return nextdata[0];
    }
    return 0;
}

int compare_string_record(Table *table, const StringRecord *a, const StringRecord *b){
    StringRecord mida = *a;
    mida.idx = 0;
    char* dataa = get_str_chunk_data_ptr(&mida.chunk);
    StringRecord midb = *b;
    midb.idx = 0;
    char* datab = get_str_chunk_data_ptr(&midb.chunk);
    if(dataa[0] > datab[0]) 
    {
        return 1;
    }
    if(dataa[0] < datab[0]) 
    {
        return -1;
    }
    while(1) 
    {
        if(has_next_char(&mida) == 1 && has_next_char(&midb) == 1) 
        {
            dataa[0] = next_char(table, &mida);
            datab[0] = next_char(table, &midb);
            if(dataa[0] > datab[0]) 
            {
                return 1;
            }
            if(dataa[0] < datab[0]) 
            {
                return -1;
            }
        }
        else
        {
            if(has_next_char(&mida) == 1)
            {
                return 1;
            }
            if(has_next_char(&midb) == 1)
            {
                return -1;
            }
            return 0;
        }
    }
    return 0;
}

RID write_string(Table *table, const char *data, off_t size) {
    short maxsize = STR_CHUNK_MAX_SIZE - sizeof(RID) - sizeof(short);
    short number;
    short tail;
    if (size % 20 == 0) 
    {
        number = (short)(size / maxsize) - 1;
        tail = 20;
    }
    else
    {
        number = (short)(size / maxsize);
        tail = size % maxsize;
    }
    StringChunk* chunk = (StringChunk*)malloc(sizeof(StringChunk));
    get_rid_block_addr(get_str_chunk_rid(chunk)) = -1;
    get_rid_idx(get_str_chunk_rid(chunk)) = 0;
    get_str_chunk_size(chunk) = calc_str_chunk_size(tail);
    for(int i = 0; i < tail; i++) 
    {
        chunk->data[sizeof(RID) + sizeof(short) + i] = data[number * maxsize + i];
    }
    RID rid = table_insert(table, chunk->data, tail + sizeof(RID) + sizeof(short));
    for(int i = 1 ; i < number; i++)
    {
        get_rid_block_addr(get_str_chunk_rid(chunk)) = get_rid_block_addr(rid);
        get_rid_idx(get_str_chunk_rid(chunk)) = get_rid_idx(rid);
        get_str_chunk_size(chunk) = calc_str_chunk_size(maxsize);
        for(int j = 0; j < maxsize; j++) 
        {
            chunk->data[sizeof(RID) + sizeof(short) + j] = data[(number - i)* maxsize + j];
        }
        rid = table_insert(table, chunk->data, STR_CHUNK_MAX_SIZE);
    }
    if(size > maxsize) 
    {
        get_rid_block_addr(get_str_chunk_rid(chunk)) = get_rid_block_addr(rid);
        get_rid_idx(get_str_chunk_rid(chunk)) = get_rid_idx(rid);
        get_str_chunk_size(chunk) = calc_str_chunk_size(maxsize);
        for(int i = 0; i < maxsize; i++) 
        {
            chunk->data[sizeof(RID) + sizeof(short) + i] = data[i];
        }
        rid = table_insert(table, chunk->data, STR_CHUNK_MAX_SIZE);
    }
    return rid;
}

void delete_string(Table *table, RID rid) {
    off_t address = get_rid_block_addr(rid);
    short idx = get_rid_idx(rid);
    Block* block = (Block*)get_page(&table->data_pool, address);
    StringChunk* chunk = (StringChunk*)get_item(block, idx);
    release(&table->data_pool, address);
    RID nextrid = get_str_chunk_rid(chunk);
    address = get_rid_block_addr(nextrid);
    idx = get_rid_idx(nextrid);
    table_delete(table, rid);
    RID mid = nextrid;
    while(address != -1)
    {
        block = (Block*)get_page(&table->data_pool, address);
        chunk = (StringChunk*)get_item(block, idx);
        release(&table->data_pool, address);
        nextrid = get_str_chunk_rid(chunk);
        table_delete(table, mid);
        address = get_rid_block_addr(nextrid);
        idx = get_rid_idx(nextrid);
        mid = nextrid;
    }
}

/* void print_string(Table *table, const StringRecord *record) {
    StringRecord rec = *record;
    printf("\"");
    while (has_next_char(&rec)) {
        printf("%c", next_char(table, &rec));
    }
    printf("\"");
} */

size_t load_string(Table *table, const StringRecord *record, char *dest, size_t max_size) {
    StringRecord mid = *record;
    mid.idx = 0;
    size_t size = 1;
    char* data = get_str_chunk_data_ptr(&mid.chunk);
    dest[0] = data[0];
    while(size < max_size) 
    {
        if(has_next_char(&mid) == 1) 
        {
            dest[size] = next_char(table, &mid);
        }
        else 
        {
            return size;
        }
        size++;
    }
    return size;
}

/* void chunk_printer(ItemPtr item, short item_size) {
    if (item == NULL) {
        printf("NULL");
        return;
    }
    StringChunk *chunk = (StringChunk*)item;
    short size = get_str_chunk_size(chunk), i;
    printf("StringChunk(");
    print_rid(get_str_chunk_rid(chunk));
    printf(", %d, \"", size);
    for (i = 0; i < size; i++) {
        printf("%c", get_str_chunk_data_ptr(chunk)[i]);
    }
    printf("\")");
} */