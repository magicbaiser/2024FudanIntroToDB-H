#include "hash_map.h"

#include <stdio.h>

void hash_table_init(const char *filename, BufferPool *pool, off_t n_directory_blocks) {
    init_buffer_pool(filename, pool);
    /* TODO: add code here */
    if(pool->file.length == 0)
    {
        Page* start;
        for(int i = 0; i < n_directory_blocks + 2; i++) 
        {
            start = get_page(pool, i * PAGE_SIZE);
            release(pool, i * PAGE_SIZE);
        }
        HashMapControlBlock* control = (HashMapControlBlock*)get_page(pool, 0);
        control->free_block_head = (n_directory_blocks + 1) * PAGE_SIZE;
        control->n_directory_blocks = n_directory_blocks;
        control->max_size = n_directory_blocks * HASH_MAP_DIR_BLOCK_SIZE;
        control->n_all_blocks = n_directory_blocks + 2;
        HashMapDirectoryBlock* direct;
        for(int i = 1; i <n_directory_blocks + 1; i++) 
        {
            direct = (HashMapDirectoryBlock*)get_page(pool, i * PAGE_SIZE);
            for(int j = 0; j < HASH_MAP_DIR_BLOCK_SIZE; j++) 
            {
                direct->directory[j] = -1;
            }
            release(pool, i * PAGE_SIZE);
        }
        off_t address = control->free_block_head;
        release(pool, 0);
        HashMapBlock* map = (HashMapBlock*)get_page(pool, address);
        map->next = 0;
        map->n_items = 0;
        for(int i = 0; i < HASH_MAP_BLOCK_SIZE; i++) 
        {
            map->table[i] = -1;
        }
        release(pool, address);
    }
}

void hash_table_close(BufferPool *pool) {
    close_buffer_pool(pool);
}

void hash_table_insert(BufferPool *pool, short size, off_t addr) {
    HashMapControlBlock* control = (HashMapControlBlock*)get_page(pool, 0);
    if(size < control->max_size) 
    {
        short number = size / HASH_MAP_DIR_BLOCK_SIZE;
        short location = size % HASH_MAP_DIR_BLOCK_SIZE;
        HashMapDirectoryBlock* direct = (HashMapDirectoryBlock*)get_page(pool, (number + 1) * PAGE_SIZE);
        if(direct->directory[location] == -1) 
        {
            off_t emptyaddress = control->free_block_head;
            if(emptyaddress == 0) 
            {
                off_t tail = control->n_all_blocks * PAGE_SIZE;
                direct->directory[location] = tail;
                HashMapBlock* map = (HashMapBlock*)get_page(pool, tail);
                map->n_items = 1;
                map->next = 0;
                map->table[0] = addr;
                for(int i = 1; i < HASH_MAP_BLOCK_SIZE; i++) 
                {
                    map->table[i] = -1;
                }
                control->n_all_blocks++;
                release(pool, tail);
            }
            else 
            {
                direct->directory[location] = emptyaddress;
                HashMapBlock* empty = (HashMapBlock*)get_page(pool, emptyaddress);
                control->free_block_head = empty->next;
                empty->n_items++;
                empty->next = 0;
                empty->table[0] = addr;
                release(pool, emptyaddress);
            }
        }
        else 
        {
            off_t mapaddress = direct->directory[location];
            HashMapBlock* map = (HashMapBlock*)get_page(pool, mapaddress);
            short cloned = 0;
            while(HASH_MAP_BLOCK_SIZE < map->n_items + 1 && map->next != 0) 
            {
                for(int i = 0; i < HASH_MAP_BLOCK_SIZE; i++) 
                {
                    if(map->table[i] == addr) 
                    {
                        cloned = 1;
                    }
                }
                release(pool, mapaddress);
                mapaddress = map->next;
                map = (HashMapBlock*)get_page(pool, mapaddress);
            }
            if(cloned != 0) 
            {
                release(pool, mapaddress);
                release(pool, (number + 1) * PAGE_SIZE);
                release(pool, 0);
                return;
            }
            if(map->n_items < HASH_MAP_BLOCK_SIZE)
            {
                for(int i = 0; i < HASH_MAP_BLOCK_SIZE; i++) 
                {
                    if(map->table[i] == -1) 
                    {
                        map->n_items++;
                        map->table[i] = addr;
                        break;
                    }
                }
            }
            else 
            {
                off_t emptyaddress = control->free_block_head;
                if(emptyaddress == 0) 
                {
                    off_t tail = control->n_all_blocks * PAGE_SIZE;
                    HashMapBlock* nextmap = (HashMapBlock*)get_page(pool, tail);
                    nextmap->n_items = 1;
                    nextmap->next = 0;
                    nextmap->table[0] = addr;
                    for(int i = 1; i < HASH_MAP_BLOCK_SIZE; i++) 
                    {
                        nextmap->table[i] = -1;
                    }
                    map->next = tail;
                    control->n_all_blocks++;
                    release(pool, tail);
                }
                else 
                {
                    HashMapBlock* empty = (HashMapBlock*)get_page(pool, emptyaddress);
                    control->free_block_head = empty->next;
                    empty->n_items++;
                    empty->next = 0;
                    empty->table[0] = addr;
                    map->next = emptyaddress;
                    release(pool, emptyaddress);
                }
            }
            release(pool, mapaddress);
        }
        release(pool, (number + 1) * PAGE_SIZE);
        release(pool, 0);
    }
    else
    {
        release(pool, 0);
    }
}

off_t hash_table_pop_lower_bound(BufferPool *pool, short size) {
    HashMapControlBlock* control = (HashMapControlBlock*)get_page(pool, 0);
    short mid = size;
    while(mid < control->max_size) 
    {
        short number = mid / HASH_MAP_DIR_BLOCK_SIZE; 
        short location = mid % HASH_MAP_DIR_BLOCK_SIZE; 
        HashMapDirectoryBlock* direct = (HashMapDirectoryBlock*)get_page(pool, (number + 1) * PAGE_SIZE);
        if(direct->directory[location] != -1)
        {
            off_t address = direct->directory[location];
            HashMapBlock* map;
            off_t nextaddress;
            while(address != 0) 
            {
                map = (HashMapBlock*)get_page(pool, address);
                nextaddress = map->next;
                release(pool, address);
                address = nextaddress;
            }
            off_t result; 
            if(map->n_items != HASH_MAP_BLOCK_SIZE) 
            {
                for(int i = 0; i < map->n_items + 1; i++){
                    if(map->table[i] == -1) 
                    {
                        result = map->table[i - 1];
                        hash_table_pop(pool, mid, result);
                        release(pool, (number + 1) * PAGE_SIZE);
                        release(pool, 0);
                        return result;
                    }
                }
            }
            else 
            {
                result = map->table[HASH_MAP_BLOCK_SIZE - 1];
                hash_table_pop(pool, mid, result);
                release(pool, (number + 1) * PAGE_SIZE);
                release(pool, 0);
                return result;
            }
        }
        release(pool, (number + 1) * PAGE_SIZE);
        mid++;
    }
    release(pool, 0);
    return -1;
}

void hash_table_pop(BufferPool *pool, short size, off_t addr) {
    HashMapDirectoryBlock* direct = (HashMapDirectoryBlock*)get_page(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    HashMapControlBlock* control = (HashMapControlBlock*)get_page(pool, 0);
    if(direct->directory[size % HASH_MAP_DIR_BLOCK_SIZE] != -1) 
    {
        off_t mapaddress = direct->directory[size % HASH_MAP_DIR_BLOCK_SIZE];
        HashMapBlock* map = (HashMapBlock*)get_page(pool, mapaddress);
        if(map->n_items == 1) 
        {
            off_t emptyaddress = control->free_block_head;
            map->next = emptyaddress;
            map->n_items = map->n_items - 1;
            map->table[0] = -1;
            direct->directory[size % HASH_MAP_DIR_BLOCK_SIZE] = -1;
            control->free_block_head = mapaddress;
            release(pool, mapaddress);
            release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
            release(pool, 0);
            return;
        }
        else
        {
            HashMapBlock* map;
            HashMapBlock* tail;
            off_t nextaddress;
            off_t tailaddress;
            do
            {
                map = (HashMapBlock*)get_page(pool, mapaddress);
                nextaddress = map->next;
                for(int i = 0; i < map->n_items; i++) 
                {
                    if(map->table[i] == addr) 
                    {
                        if(i == map->n_items - 1) 
                        {
                            map->table[i] = -1;
                        }
                        else
                        {
                            map->table[i] = map->table[map->n_items - 1];
                            map->table[map->n_items - 1] = -1;
                        }
                        if(map->n_items == 1) 
                        {
                            tail = (HashMapBlock*)get_page(pool, tailaddress);
                            tail->next = nextaddress;
                            map->next = control->free_block_head;
                            control->free_block_head = mapaddress;
                            release(pool, tailaddress);
                        }
                        map->n_items = map->n_items - 1;
                        release(pool, mapaddress);
                        release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
                        release(pool, 0);
                        return;
                    }
                }
                release(pool, mapaddress);
                tailaddress = mapaddress;
                mapaddress = nextaddress;
            }
            while(map->next != 0);
        }
        release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
        release(pool, 0);
    }
    else
    {
        release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
        release(pool, 0);
    }
}

/* void print_hash_table(BufferPool *pool) {
    HashMapControlBlock *ctrl = (HashMapControlBlock*)get_page(pool, 0);
    HashMapDirectoryBlock *dir_block;
    off_t block_addr, next_addr;
    HashMapBlock *block;
    int i, j;
    printf("----------HASH TABLE----------\n");
    for (i = 0; i < ctrl->max_size; ++i) {
        dir_block = (HashMapDirectoryBlock*)get_page(pool, (i / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
        if (dir_block->directory[i % HASH_MAP_DIR_BLOCK_SIZE] != 0) {
            printf("%d:", i);
            block_addr = dir_block->directory[i % HASH_MAP_DIR_BLOCK_SIZE];
            while (block_addr != 0) {
                block = (HashMapBlock*)get_page(pool, block_addr);
                printf("  [" FORMAT_OFF_T "]", block_addr);
                printf("{");
                for (j = 0; j < block->n_items; ++j) {
                    if (j != 0) {
                        printf(", ");
                    }
                    printf(FORMAT_OFF_T, block->table[j]);
                }
                printf("}");
                next_addr = block->next;
                release(pool, block_addr);
                block_addr = next_addr;
            }
            printf("\n");
        }
        release(pool, (i / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    }
    release(pool, 0);
    printf("------------------------------\n");
} */