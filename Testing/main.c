#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libtdmm/tdmm.h"
#include "get_stats.c"

static void ASSERT(int cond, const char* msg) {
    if(!cond) {
        printf("  FAIL: %s\n", msg);
        exit(1);
    }
}

void test_basic_malloc_free(alloc_strat_e strat, const char* name) {
    printf("=== %s: basic malloc/free ===\n", name);
    t_init(strat);

    int* p = (int*)t_malloc(sizeof(int));
    ASSERT(p != NULL, "t_malloc returned NULL");
    
    srand(time(NULL));
    
    int val = rand() % 1000000;
    *p = val;
    ASSERT(*p == val, "basic malloc failed");

    t_free(p);
    printf("\n");
}

void test_multiple_allocs(alloc_strat_e strat, const char* name) {
    printf("=== %s: multiple allocations ===\n", name);
    t_init(strat);

    int* a = (int*)t_malloc(sizeof(int));
    int* b = (int*)t_malloc(sizeof(int));
    int* c = (int*)t_malloc(sizeof(int));
    
    srand(time(NULL));
    int a_val = rand() % 1000000;
    int b_val = rand() % 1000000;
    int c_val = rand() % 1000000;
    *a = a_val; *b = b_val; *c = c_val;

    ASSERT(*a == a_val, "first allocation value corrupted");
    ASSERT(*b == b_val, "second allocation value corrupted");
    ASSERT(*c == c_val, "third allocation value corrupted");
    ASSERT(a != b && b != c && a != c, "pointers should be unique");

    t_free(a);
    t_free(b);
    t_free(c);
    printf("\n");
}

void test_large_allocation(alloc_strat_e strat, const char* name) {
    printf("=== %s: large allocation ===\n", name);
    t_init(strat);

    size_t count = 1024;
    int* arr = (int*)t_malloc(count * sizeof(int));
    ASSERT(arr != NULL, "large t_malloc returned NULL");

    for(size_t i = 0; i < count; i++) arr[i] = (int)i;

    bool good = true;
    for(size_t i = 0; i < count; i++) {
        if(arr[i] != (int)i) {  
            good = false; 
            break; 
        }
    }
    ASSERT(good, "large allocation data corrupted");

    t_free(arr);
    printf("\n");
}

void test_string_data(alloc_strat_e strat, const char* name) {
    printf("=== %s: string data ===\n", name);
    t_init(strat);

    char* s = (char*)t_malloc(64);
    ASSERT(s != NULL, "t_malloc for string returned NULL");

    strcpy(s, "hello world");
    ASSERT(strcmp(s, "hello world") == 0, "string data corrupted");

    t_free(s);
    printf("\n");
}

void test_free_null(alloc_strat_e strat, const char* name) {
    printf("=== %s: free NULL ===\n", name);
    t_init(strat);

    t_free(NULL);
    ASSERT(true, "free(NULL) should not crash");
    printf("\n");
}

void test_zero_malloc(alloc_strat_e strat, const char* name) {
    printf("=== %s: malloc(0) ===\n", name);
    t_init(strat);

    void* p = t_malloc(0);
    ASSERT(p == NULL, "t_malloc(0) should return NULL");
    printf("\n");
}

void test_many_small_allocs(alloc_strat_e strat, const char* name) {
    printf("=== %s: many small allocations ===\n", name);
    t_init(strat);

    #define SMALL_COUNT 100
    char* ptrs[SMALL_COUNT];

    for(int i = 0; i < SMALL_COUNT; i++) {
        ptrs[i] = (char*)t_malloc(8);
        ASSERT(ptrs[i] != NULL, "small alloc returned NULL");
        ptrs[i][0] = (char)i;
    }

    int data_ok = 1;
    for(int i = 0; i < SMALL_COUNT; i++) {
        if(ptrs[i][0] != (char)i) { data_ok = 0; break; }
    }
    ASSERT(data_ok, "small allocation data corrupted across many allocs");

    for(int i = 0; i < SMALL_COUNT; i++) t_free(ptrs[i]);
    printf("\n");
}

void test_interleaved_malloc_free(alloc_strat_e strat, const char* name) {
    printf("=== %s: interleaved malloc/free ===\n", name);
    t_init(strat);

    int* a = (int*)t_malloc(sizeof(int)); *a = 1;
    int* b = (int*)t_malloc(sizeof(int)); *b = 2;
    t_free(a);
    int* c = (int*)t_malloc(sizeof(int)); *c = 3;
    t_free(b);
    int* d = (int*)t_malloc(sizeof(int)); *d = 4;

    ASSERT(*c == 3, "value after interleaved ops corrupted (c)");
    ASSERT(*d == 4, "value after interleaved ops corrupted (d)");

    t_free(c);
    t_free(d);
    printf("\n");
}

void test_varying_sizes(alloc_strat_e strat, const char* name) {
    printf("=== %s: varying sizes ===\n", name);
    t_init(strat);

    char* tiny = (char*)t_malloc(1);
    char* small = (char*)t_malloc(64);
    char* medium = (char*)t_malloc(1024);
    char* large = (char*)t_malloc(4096);
    char* largest = (char*)t_malloc(8192);

    ASSERT(tiny != NULL, "1-byte alloc returned NULL");
    ASSERT(small != NULL, "64-byte alloc returned NULL");
    ASSERT(medium != NULL, "1024-byte alloc returned NULL");
    ASSERT(large != NULL, "4096-byte alloc returned NULL");
    ASSERT(largest != NULL, "8192-byte alloc returned NULL");

    *tiny = 'A';
    memset(small, 'B', 64);
    memset(medium, 'C', 1024);
    memset(large, 'D', 4096);
    memset(largest, 'E', 8192);

    ASSERT(*tiny == 'A', "tiny value corrupted");
    ASSERT(small[63] == 'B', "small value corrupted");
    ASSERT(medium[1023] == 'C', "medium value corrupted");
    ASSERT(large[4095] == 'D', "large value corrupted");
    ASSERT(largest[8191] == 'E', "largest value corrupted");

    t_free(tiny);
    t_free(small);
    t_free(medium);
    t_free(large);
    t_free(largest);
    printf("\n");
}

void run_all_tests_for_strategy(alloc_strat_e strat, const char* name) {
    printf("########## Testing %s ##########\n\n", name);
    test_basic_malloc_free(strat, name);
    test_multiple_allocs(strat, name);
    test_large_allocation(strat, name);
    test_string_data(strat, name);
    test_free_null(strat, name);
    test_zero_malloc(strat, name);
    test_many_small_allocs(strat, name);
    test_interleaved_malloc_free(strat, name);
    test_varying_sizes(strat, name);
}

int main() {
    run_all_tests_for_strategy(FIRST_FIT, "First_Fit");
    run_all_tests_for_strategy(BEST_FIT, "Best_Fit");
    run_all_tests_for_strategy(WORST_FIT, "Worst_Fit");
    run_all_tests_for_strategy(MIXED, "Mixed");
    run_all_tests_for_strategy(BUDDY, "Buddy");

    printf("========================================\n");

    run_all_benchmarks();

    return 0;
}
