#include "buffer_pool.h"
#include "file_io.h"

#include <stdio.h>
#include <stdlib.h>

void init_buffer_pool(const char *filename, BufferPool *pool) {
    open_file(&pool->file, filename);
    for(int i = 0; i < CACHE_PAGE; i++) 
    {
        pool->addrs[i] = -1;
        pool->cnt[i] = 0;
        pool->ref[i] = 0;
        pool->empty[i] = 1;
        read_page(&pool->pages[i], &pool->file, PAGE_SIZE * i);
    }
}

void close_buffer_pool(BufferPool *pool) {
    for(int i = 0; i < CACHE_PAGE; i++) 
    {
        write_page(&pool->pages[i], &pool->file, pool->addrs[i]);
    }
    close_file(&pool->file);
}

Page* get_page(BufferPool *pool, off_t addr) {
    int number = -1;
    for(int i = 0 ; i < CACHE_PAGE; i++)
    {
        pool->cnt[i]++;
    }
    for(int i = 0; i < CACHE_PAGE; i++) 
    {
        if(pool->empty[i] == 1) 
        {
            number = i;
            read_page(&pool->pages[i], &pool->file, addr);
            pool->addrs[number] = addr;
            pool->ref[number]++;
            pool->cnt[number] = 0;
            pool->empty[number] = 0;
            break;
        }
        if(pool->addrs[i] == addr) 
        {
            number = i;
            pool->ref[i]++;
            pool->cnt[i] = 0;
            pool->addrs[i] = addr;
            pool->empty[i] = 0;
            break;
        }
    }
    int change = -1;
    if(number == -1) 
    {
        size_t max = 0;
        for(int i = 0; i < 7; i++) 
        {
            if (pool->cnt[i] > max) 
            {
                change = i;
                max = pool->cnt[i];
            }
        }
        release(pool, pool->addrs[7]);
        pool->addrs[7] = addr;
        pool->ref[7]++;
        pool->cnt[7] = 0;
        pool->empty[7] = 0;
        return &pool->pages[7];
    }
    if(pool->ref[7] > 1)
    {
        size_t midaddr = pool->addrs[7];
        size_t midref = pool->ref[7];
        size_t midcnt = pool->cnt[7];
        pool->addrs[7] = pool->addrs[change];
        pool->ref[7] = 1;
        pool->cnt[7] = pool->cnt[change];
        pool->addrs[change] = midaddr;
        pool->ref[change] = midref;
        pool->cnt[change] = midcnt; 
    }    
    if(number == 7)
    {
        return &pool->pages[change];
    }
    return &pool->pages[number];
}

void release(BufferPool *pool, off_t addr) {
    for(int i = 0; i < CACHE_PAGE; i++) 
    {
        if(pool->addrs[i] == addr) 
        {
            write_page(&pool->pages[i], &pool->file, pool->addrs[i]);
            pool->addrs[i] = -1;
            pool->ref[i] = 0;
            pool->empty[i] = 1;
        }
    }
}

/* void print_buffer_pool(BufferPool *pool) {
} */

/* void validate_buffer_pool(BufferPool *pool) {
} */
