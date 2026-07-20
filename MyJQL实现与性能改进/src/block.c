#include "block.h"

#include <stdio.h>
#include <stdlib.h>

void init_block(Block *block) {
    block->n_items = 0;
    block->head_ptr = (short)(block->data - (char*)block);
    block->tail_ptr = (short)sizeof(Block);
}

ItemPtr get_item(Block *block, short idx) {
    if (idx < 0 || idx >= block->n_items) {
        printf("get item error: idx is out of range\n");
        return NULL;
    }
    ItemID item_id = get_item_id(block, idx);
    if (get_item_id_availability(item_id)) {
        printf("get item error: item_id is not used\n");
        return NULL;
    }
    short offset = get_item_id_offset(item_id);
    return (char*)block + offset;
}

short new_item(Block *block, ItemPtr item, short item_size) {
    short space = block->tail_ptr - block->head_ptr;
    if(item_size > space) 
    {
        return -1;
    }
    ItemID id;
    short number = block->n_items;
    for(short i = 0; i < number; i++) 
    {
        id = get_item_id(block, i);
        if(get_item_id_availability(id) == 1 && get_item_id_size(id) >= (unsigned short)item_size) 
        {
            short offset = get_item_id_offset(id);
            for(short j = 0; j < item_size; j++) 
            {
                block->data[offset - 3 * sizeof(short) + j] = item[j];
            }
            block->tail_ptr = block->tail_ptr - item_size;
            get_item_id(block, i) = compose_item_id(0, block->tail_ptr, item_size);
            return i;
        }
    }
    if(item_size + sizeof(ItemID) > space) 
    {
        return -1;
    }
    block->n_items = block->n_items + 1;
    block->tail_ptr = block->tail_ptr - item_size;
    get_item_id(block, number) = compose_item_id(0, block->tail_ptr, item_size);
    id = get_item_id(block, number);
    short offset = get_item_id_offset(id);
    for(short i = 0; i < item_size; i++) 
    {
        block->data[offset - 3 * sizeof(short) + i] = item[i];
    }
    block->head_ptr = block->head_ptr + sizeof(ItemID);
    return number;
}

void delete_item(Block *block, short idx) {
    if(idx >= 0 && idx <= block->n_items) 
    {
        ItemID id = get_item_id(block, idx);
        short size = get_item_id_size(id);
        short availability = get_item_id_availability(id);
        if(availability == 0) 
        {
            if(size == 0)
            {
                for(int i = 1; i < sizeof(ItemID) + 1; i++) 
                {
                    block->data[block->head_ptr - 3 * sizeof(short) - i] = 0;
                }
                block->n_items = block->n_items - 1;
                block->head_ptr = block->head_ptr - sizeof(ItemID);
            }
            else
            {
                ItemID another;
                short offset = get_item_id_offset(id);
                for(int i = 0; i < block->n_items; i++) 
                {
                    another = get_item_id(block, i);
                    if(get_item_id_availability(another) == 0 && get_item_id_offset(another) < (unsigned short)offset) 
                    {
                        short another_offset = get_item_id_offset(another) + size;
                        short another_size = get_item_id_size(another);
                        get_item_id(block, i) = compose_item_id(0, another_offset, another_size);
                    }
                }
                ItemPtr item = get_item(block, idx);
                short number = (short)item - (short)block - block->tail_ptr;
                char* mid = (char*)malloc(number * sizeof(char));
                for(int i = 0; i < number; i++) 
                {
                    mid[i] = block->data[block->tail_ptr - 3 * sizeof(short) + i];
                }
                for(int i = 0; i < size; i++) 
                {
                    block->data[block->tail_ptr - 3 * sizeof(short) + i] = 0;
                }
                block->tail_ptr = block->tail_ptr + size;
                for(int i = 0; i < number; i++) 
                {
                    block->data[block->tail_ptr - 3 * sizeof(short) + i] = mid[i];
                }
                if(idx < block->n_items - 1) 
                {
                    get_item_id(block, idx) = compose_item_id(1, 0, 0);
                }
                else
                {
                    for(int i = 1; i < sizeof(ItemID) + 1; i++) 
                    { 
                        block->data[block->head_ptr - 3 * sizeof(short) - i] = 0;
                    }
                    block->n_items = block->n_items - 1;
                    block->head_ptr = block->head_ptr - sizeof(ItemID);
                }
            }
        }
    }
}

/* void str_printer(ItemPtr item, short item_size) {
    if (item == NULL) {
        printf("NULL");
        return;
    }
    short i;
    printf("\"");
    for (i = 0; i < item_size; ++i) {
        printf("%c", item[i]);
    }
    printf("\"");
}

void print_block(Block *block, printer_t printer) {
    short i, availability, offset, size;
    ItemID item_id;
    ItemPtr item;
    printf("----------BLOCK----------\n");
    printf("total = %d\n", block->n_items);
    printf("head = %d\n", block->head_ptr);
    printf("tail = %d\n", block->tail_ptr);
    for (i = 0; i < block->n_items; ++i) {
        item_id = get_item_id(block, i);
        availability = get_item_id_availability(item_id);
        offset = get_item_id_offset(item_id);
        size = get_item_id_size(item_id);
        if (!availability) {
            item = get_item(block, i);
        } else {
            item = NULL;
        }
        printf("%10d%5d%10d%10d\t", i, availability, offset, size);
        printer(item, size);
        printf("\n");
    }
    printf("-------------------------\n");
}

void analyze_block(Block *block, block_stat_t *stat) {
    short i;
    stat->empty_item_ids = 0;
    stat->total_item_ids = block->n_items;
    for (i = 0; i < block->n_items; ++i) {
        if (get_item_id_availability(get_item_id(block, i))) {
            ++stat->empty_item_ids;
        }
    }
    stat->available_space = block->tail_ptr - block->head_ptr 
        + stat->empty_item_ids * sizeof(ItemID);
}

void accumulate_stat_info(block_stat_t *stat, const block_stat_t *stat2) {
    stat->empty_item_ids += stat2->empty_item_ids;
    stat->total_item_ids += stat2->total_item_ids;
    stat->available_space += stat2->available_space;
}

void print_stat_info(const block_stat_t *stat) {
    printf("==========STAT==========\n");
    printf("empty_item_ids: " FORMAT_SIZE_T "\n", stat->empty_item_ids);
    printf("total_item_ids: " FORMAT_SIZE_T "\n", stat->total_item_ids);
    printf("available_space: " FORMAT_SIZE_T "\n", stat->available_space);
    printf("========================\n");
} */