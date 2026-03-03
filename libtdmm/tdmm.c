#include "tdmm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define IS_FREE(x) ((x)->size & 1)
#define SET_FREE(x, y) ((x)->size = ((x)->size & ~1ULL) | (y))

#define GET_SIZE(x) ((x)->size & ~1ULL)
#define SET_SIZE(x, y) ((x)->size = (y) | IS_FREE(x))

#define MAX_BUDDY_ORDER 24

static header* buddy_free_lists[MAX_BUDDY_ORDER];
static header* headers_start;
static header *headers_end;
static size_t requested_size;
static size_t total_size;
static alloc_strat_e strategy;
static long page_size;
static size_t data_structure_overhead;
static int mixed_counter;

static inline void set_block_state(header* block, int is_free, size_t size, header* next, header* prev) {
    block->size = size | is_free;
    block->next = next;
    block->prev = prev;
}

static void split_block(header* block, size_t size) {
    size_t block_size = GET_SIZE(block);
    if (block_size < size + sizeof(header) + 4) return;
    
    header* new_block = (header*)((char*)block + sizeof(header) + size);
    set_block_state(new_block, true, block_size - size - sizeof(header), block->next, block);
    
    block->size = size | (block->size & 1);
    block->next = new_block;
    
    if(headers_end == block) headers_end = new_block;
    data_structure_overhead += sizeof(header);
}

static inline header* find_free_block(size_t size) {
    if(size < 1) return NULL;
    
    if(strategy != BUDDY){
        if(strategy == MIXED) {
            strategy = mixed_counter;
            mixed_counter = (mixed_counter + 1) % 3;
        }
        
        header* current = headers_start;
        header* rslt = NULL;
    
        while(current != NULL) {
            size_t s = GET_SIZE(current);
            if(IS_FREE(current) && s >= size) {
                if(strategy == FIRST_FIT) {
                    return current;
                } else if(strategy == BEST_FIT) {
                    if(rslt == NULL || s < GET_SIZE(rslt)) rslt = current;
                } else if(strategy == WORST_FIT) {
                    if(rslt == NULL || s > GET_SIZE(rslt)) rslt = current;
                }
            }
            current = current->next;
        }
    
        return rslt;
    } else{
        //TODO: implement buddy allocation finding here
        size_t bucket_index = size == 1 ? 0 : 1ULL << (sizeof(size_t)*8 - __builtin_clzl(size-1));
    }
}

void t_init(alloc_strat_e strat) {
	strategy = strat;
	if(strat == MIXED) mixed_counter = 0;
	
	page_size = sysconf(_SC_PAGESIZE);
	header* initial_block = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    if(initial_block == MAP_FAILED) {
        fprintf(stderr, "Error: failed to initialize allocator\n");
        exit(0);
    }
    
    set_block_state(initial_block, true, page_size - sizeof(header), NULL, NULL);
    
    headers_start = initial_block;
    headers_end = initial_block;
    requested_size = 0;
    total_size = page_size;
    data_structure_overhead = sizeof(header);
}

void *t_malloc(size_t size) {
    if(size == 0) return NULL;
    
    size_t aligned_size = (size + 3) & ~3;
    
    header* block = find_free_block(aligned_size);
    if(block == NULL) {
        size_t size_needed = aligned_size + sizeof(header);
        size_t allocation_size = (size_needed + page_size - 1) & ~(page_size - 1);
        void* endptr = (char*)headers_end + sizeof(header) + GET_SIZE(headers_end);
        header* new_block = mmap(endptr, allocation_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        
        if(new_block == MAP_FAILED) {
            fprintf(stderr, "Error: failed to allocate memory\n");
            exit(1);
        }
        
        set_block_state(new_block, true, allocation_size - sizeof(header), NULL, headers_end);
        
        headers_end->next = new_block;
        headers_end = new_block;
        block = new_block;
        
        total_size += allocation_size;
        data_structure_overhead += sizeof(header);
    }
    
    split_block(block, aligned_size);
    requested_size += aligned_size;
    return (char*)block + sizeof(header);
}

static void merge_blocks(header* block){
    if(!block || !block->next) return;
    if((char*)block->next != (char*)block + sizeof(header) + GET_SIZE(block)) return;
    if(!IS_FREE(block) || !IS_FREE(block->next)) return;
    if(strategy == BUDDY && (GET_SIZE(block) != GET_SIZE(block->next))) return;
    
    if(block->next == headers_end) headers_end = block;
    
    block->size += sizeof(header) + GET_SIZE(block->next);
    block->next = block->next->next;
    data_structure_overhead -= sizeof(header);
}

void t_free(void *ptr) {
   	if(ptr == NULL) return;
    
    header* block = (header*)((char*)ptr - sizeof(header));
    SET_FREE(block, 1);
    requested_size -= GET_SIZE(block);
    
    merge_blocks(block);
    merge_blocks(block->prev);
}

void t_display_stats() {
    printf("Total bytes requested from sys: %zu bytes\n", total_size);
    printf("Data structure overhead: %zu bytes (%.5f%%)\n", data_structure_overhead, (double)data_structure_overhead / total_size * 100);
}

size_t t_get_ds_overhead() {return data_structure_overhead;}
double t_get_usage(){ return (double)requested_size / total_size * 100;}