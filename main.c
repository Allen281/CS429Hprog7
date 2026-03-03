#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tdmm.h"

#define NUM_OPERATIONS 10000

double get_elapsed_time(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) * 1e9 + (double)(end.tv_nsec - start.tv_nsec);
}

void run_benchmarks_for_policy(alloc_strat_e strat, const char* policy_name, FILE* average_util, FILE* overhead) {
    char speed_filename[256];
    char util_filename[256];

    sprintf(speed_filename, "speed_%s.csv", policy_name);
    sprintf(util_filename, "utilization_%s.csv", policy_name);

    FILE* speed_graph = fopen(speed_filename, "w");
    FILE* util_graph = fopen(util_filename, "w");

    if (!speed_graph || !util_graph) {
        printf("Failed to open output files for %s\n", policy_name);
        return;
    }
    fprintf(speed_graph, "Size(Bytes),MallocTime(ns),FreeTime(ns)\n");
    fprintf(util_graph, "Time(ns),Utilization(%%)\n");

    struct timespec start, end, start_time, current_time;
    const size_t MAX_SIZE = 1024*1024*8;

    for (size_t size = 1; size <= MAX_SIZE; size *= 2) {
        t_init(strat);

        clock_gettime(CLOCK_MONOTONIC, &start);
        void* ptr = t_malloc(size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double malloc_time = get_elapsed_time(start, end);

        clock_gettime(CLOCK_MONOTONIC, &start);
        t_free(ptr);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double free_time = get_elapsed_time(start, end);

        fprintf(speed_graph, "%zu,%.0f,%.0f\n", size, malloc_time, free_time);
    }

    t_init(strat);
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    double average_utilization = 0.0;
    void* allocated[NUM_OPERATIONS] = {NULL};

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        int is_alloc = (rand() % 100 < 70);
        
        if(i == 0) is_alloc = 1;

        if(is_alloc){
            size_t alloc_size = (rand() % (1024*1024*8)) + 1;
            allocated[i] = t_malloc(alloc_size);
        } else {
            int index = rand() % i;
            int counter = 0;
            while(allocated[index] == 0 && counter < i){
                index = rand() % i;
                counter++;
            }
            
            t_free(allocated[index]);
            allocated[index] = NULL;
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double elapsed = get_elapsed_time(start_time, current_time);
        double current_util = t_get_usage();
        average_utilization += current_util;

        fprintf(util_graph, "%.0f,%.2f\n", elapsed, current_util);
    }

    fclose(speed_graph);
    fclose(util_graph);
    
    fprintf(average_util, "%s,%.2f\n", policy_name, average_utilization/NUM_OPERATIONS);
    fprintf(overhead, "%s,%zu\n", policy_name, t_get_ds_overhead());

    // Print required console statistics
    printf("Results for %s: \n", policy_name);
    printf("Average Memory Utilization: %.2f%%\n", average_utilization/NUM_OPERATIONS);
    t_display_stats();
    printf("\n");
}

int main() {
    int random = time(NULL);
    srand(random);
    
    FILE* average_util = fopen("average_utilization.csv", "w");
    FILE* overhead = fopen("overhead.csv", "w");

    run_benchmarks_for_policy(FIRST_FIT, "First_Fit", average_util, overhead);

    srand(random);
    run_benchmarks_for_policy(BEST_FIT,  "Best_Fit", average_util, overhead);

    srand(random);
    run_benchmarks_for_policy(WORST_FIT, "Worst_Fit", average_util, overhead);
    
    fclose(average_util);
    fclose(overhead);

    printf("Benchmarking complete. CSV files generated successfully.\n");
    return 0;
}
