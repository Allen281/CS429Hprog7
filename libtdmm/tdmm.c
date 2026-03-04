#include "tdmm.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define IS_FREE(x) ((x)->size & 1)
#define SET_FREE(x, y) ((x)->size = ((x)->size & ~1ULL) | (y))

#define GET_SIZE(x) ((x)->size & ~3ULL)
#define SET_SIZE(x, y) ((x)->size = (y) | IS_FREE(x))

#define MAX_BUDDY_ORDER 24

static header* buddy_lists[MAX_BUDDY_ORDER];
static header* headers_start;
static header *headers_end;
static size_t requested_size;
static size_t total_size;
static alloc_strat_e strategy;
static long page_size;
static size_t data_structure_overhead;
static int mixed_counter;

static inline header* make_new_block(void* ptr, size_t size){
    header* block = mmap(ptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    if(block == MAP_FAILED) {
        fprintf(stderr, "Error: failed to initialize allocator\n");
        exit(0);
    }
    
    return block;
}

static inline int get_buddy_index(size_t size) {
    return size == 1 ? 0 : sizeof(size_t)*8 - __builtin_clzl(size-1) - 1;
}

static inline void remove_from_buddy_bucket(header* block) {
    int index = get_buddy_index(GET_SIZE(block) + sizeof(header));
    if(buddy_lists[index] == block) {
        buddy_lists[index] = block->next;
    }

    if(block->prev) block->prev->next = block->next;
    if(block->next) block->next->prev = block->prev;
}

static inline void add_to_buddy_bucket(header* block) {
    int index = get_buddy_index(GET_SIZE(block) + sizeof(header));
    block->next = buddy_lists[index];

    if(buddy_lists[index]) buddy_lists[index]->prev = block;

    buddy_lists[index] = block;
    block->prev = NULL;
}

static inline void set_block_state(header* block, int is_free, size_t size, header* next, header* prev) {
    SET_SIZE(block, size);
    SET_FREE(block, is_free);
    block->next = next;
    block->prev = prev;
}

static inline void split_block(header* block, size_t total_size) {
    if(strategy != BUDDY) {
        size_t block_size = GET_SIZE(block)+sizeof(header);
        if (block_size < total_size + sizeof(header)) return;
        
        header* new_block = (header*)((char*)block + total_size);
        set_block_state(new_block, true, block_size - total_size - sizeof(header), block->next, block);
        
        SET_SIZE(block, total_size-sizeof(header));
        block->next = new_block;
        if(new_block->next) new_block->next->prev = new_block;
        
        if(headers_end == block) headers_end = new_block;
        data_structure_overhead += sizeof(header);
    } else {
        remove_from_buddy_bucket(block);
        
        size_t current_size = GET_SIZE(block) + sizeof(header);
        
        while(total_size <= (current_size >> 1) && (current_size >> 1) > sizeof(header)) {
            current_size >>= 1;
            
            header* buddy_block = (header*)((char*)block + current_size);
            set_block_state(buddy_block, true, current_size - sizeof(header), NULL, NULL);
            buddy_block->block_start = block->block_start;
            
            add_to_buddy_bucket(buddy_block);
            data_structure_overhead += sizeof(header);
        }
        
        SET_FREE(block, 0);
    }
}


static inline header* find_free_block(size_t size) {
    if(size < 1) return NULL;
    
    if(strategy != BUDDY){
        alloc_strat_e current_strategy = strategy;
        
        if(strategy == MIXED) {
            current_strategy = mixed_counter;
            mixed_counter = (mixed_counter + 1) % 3;
        }
        
        header* current = headers_start;
        header* rslt = NULL;
    
        while(current != NULL) {
            size_t s = GET_SIZE(current) + sizeof(header);
            if(IS_FREE(current) && s >= size) {
                if(current_strategy == FIRST_FIT) {
                    return current;
                } else if(current_strategy == BEST_FIT) {
                    if(rslt == NULL || s < GET_SIZE(rslt) + sizeof(header)) rslt = current;
                } else if(current_strategy == WORST_FIT) {
                    if(rslt == NULL || s > GET_SIZE(rslt) + sizeof(header)) rslt = current;
                }
            }
            current = current->next;
        }
    
        return rslt;
    } else{
        size_t bucket_index = get_buddy_index(size);

        for(int i = bucket_index; i < MAX_BUDDY_ORDER; i++) {
            header* current = buddy_lists[i];
            
            while(current) {
                if(IS_FREE(current)) return current;
                current = current->next;
            }
        }
        
        return NULL;
    }
}

void t_init(alloc_strat_e strat) {
	strategy = strat;
	if(strat == MIXED) mixed_counter = 0;

    page_size = sysconf(_SC_PAGESIZE);
    header* initial_block = make_new_block(NULL, page_size);
    data_structure_overhead = sizeof(header);
    
    if(strategy == BUDDY) {
        memset(buddy_lists, 0, sizeof(buddy_lists));
        
        size_t current_size = page_size/2;
        while(current_size > sizeof(header)) {
            header* buddy_block = (header*)((char*)initial_block + current_size);
            set_block_state(buddy_block, true, current_size - sizeof(header), NULL, NULL);
            buddy_block->block_start = initial_block;
            
            add_to_buddy_bucket(buddy_block);
            current_size /= 2;
            data_structure_overhead += sizeof(header);
        }
        
        set_block_state(initial_block, true, (current_size*2)-sizeof(header), NULL, NULL);
        initial_block->block_start = initial_block;
        add_to_buddy_bucket(initial_block);
    } else {
        set_block_state(initial_block, true, page_size - sizeof(header), NULL, NULL);
    }
    
    
    total_size = page_size;
    headers_start = initial_block;
    headers_end = initial_block;
    requested_size = 0;
}

void *t_malloc(size_t size) {
    if(size == 0) return NULL;
    
    size_t aligned_size = (size+sizeof(header) + 3) & ~3;
    
    header* block = find_free_block(aligned_size);
    if(block == NULL) {
        size_t allocation_size;
        
        if(strategy == BUDDY) {
            allocation_size = (1 << (get_buddy_index(aligned_size) + 1));
            allocation_size = (allocation_size + page_size - 1) & ~(page_size - 1);
            header* new_block = make_new_block(NULL, allocation_size);
            
            set_block_state(new_block, true, allocation_size - sizeof(header), NULL, NULL);
            new_block->block_start = new_block;
            
            block = new_block;
        } else {
            allocation_size = (aligned_size + page_size - 1) & ~(page_size - 1);
            void* endptr = (char*)headers_end + sizeof(header) + GET_SIZE(headers_end);
            header* new_block = make_new_block(endptr, allocation_size);
            
            set_block_state(new_block, true, allocation_size - sizeof(header), NULL, headers_end);
            headers_end->next = new_block;
            headers_end = new_block;
            block = new_block;
        }

        total_size += allocation_size;
        data_structure_overhead += sizeof(header);
    }
    
    SET_FREE(block, 0);
    split_block(block, aligned_size);
    requested_size += GET_SIZE(block);
    return (char*)block + sizeof(header);
}

static void merge_buddy_blocks(header* block) {
    if(!block || block == block->block_start) return;
    
    size_t block_size = GET_SIZE(block);
    size_t offset = (char*)block - (char*)block->block_start;
    header* buddy = (header*)((char*)block->block_start + (offset ^ (block_size+ sizeof(header))));
    
    if(block->block_start != buddy->block_start) return;
    if(!IS_FREE(buddy) || GET_SIZE(buddy) != block_size) return;

    remove_from_buddy_bucket(block);
    remove_from_buddy_bucket(buddy);
    
    if((size_t)block < (size_t)buddy) {
        block->size += sizeof(header) + block_size;

        add_to_buddy_bucket(block);
        merge_buddy_blocks(block);
    } else {
        buddy->size += sizeof(header) + block_size;
        buddy->next = block->next;

        add_to_buddy_bucket(buddy);
        merge_buddy_blocks(buddy);
    }
    
    data_structure_overhead -= sizeof(header);
}

static inline void merge_blocks(header* block){
    if(!block || !block->next) return;
    if((char*)block->next != (char*)block + sizeof(header) + GET_SIZE(block)) return;
    if(!IS_FREE(block) || !IS_FREE(block->next)) return;
    if(strategy == BUDDY && (GET_SIZE(block) != GET_SIZE(block->next))) return;
    
    if(block->next == headers_end) headers_end = block;
    
    block->size += sizeof(header) + GET_SIZE(block->next);
    block->next = block->next->next;
    if(block->next) block->next->prev = block;
    
    data_structure_overhead -= sizeof(header);
}

void t_free(void *ptr) {
   	if(ptr == NULL) return;
    
    header* block = (header*)((char*)ptr - sizeof(header));
    SET_FREE(block, 1);
    requested_size -= GET_SIZE(block);
    
    if(strategy != BUDDY) {
        merge_blocks(block);
        merge_blocks(block->prev);
    } else {
        merge_buddy_blocks(block);
    }
}

void t_display_stats() {
    printf("Total bytes requested from sys: %zu bytes\n", total_size);
    printf("Data structure overhead: %zu bytes (%.5f%%)\n", data_structure_overhead, (double)data_structure_overhead / total_size * 100);
}

size_t t_get_ds_overhead() {return data_structure_overhead;}
double t_get_usage(){ return (double)requested_size / total_size * 100;}