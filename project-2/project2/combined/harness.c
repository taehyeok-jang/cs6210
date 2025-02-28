#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "mpi_barrier.h"
#include "omp_barrier.h"
#include "combined_barrier.h"

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int num_threads = 1;
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    }
    omp_set_num_threads(num_threads);

    gtmpi_init(size);
    gtmp_init(num_threads);

    // Performance + Sanity Test
    int iterations = 100;
    double start_time, end_time;
    int failed = 0; 

    // warm-up 
    #pragma omp parallel
    {
        for (int i = 0; i < 10; i++) {
            combined_barrier();
        }
    }

    // set timer after synchronizing all processes
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int local_flag = 0;

        for (int i = 0; i < iterations; i++) {

            if (rank == 0) {
                printf("at iteration %d\n", i);
              }

            local_flag = 0;
            #pragma omp atomic
            local_flag += 1;

            // printf("Before barrier: Rank %d, Thread %d, local_flag = %d\n", rank, tid, local_flag);
            combined_barrier();

            // printf("After barrier: Rank %d, Thread %d, local_flag = %d\n", rank, tid, local_flag);

            if (local_flag != 1) {
                #pragma omp critical
                printf("Sanity Test Failed at iteration %d: Rank %d, Thread %d, local_flag = %d (expected 1)\n",
                       i, rank, tid, local_flag);
                failed = 1;
            }
        }
    }

    // verification for global_flag 
    int local_sum = num_threads; 
    int global_flag = 0;
    MPI_Reduce(&local_sum, &global_flag, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    end_time = omp_get_wtime();

    if (rank == 0) {
        int expected = size * num_threads;
        if (global_flag != expected || failed) {
            printf("Sanity Test Failed! global_flag = %d (expected %d), failed = %d\n",
                   global_flag, expected, failed);
        } else {
            printf("Sanity Test Passed! global_flag = %d\n", global_flag);
        }

        double total_time = end_time - start_time;
        printf("Combined Barrier Performance:\n");
        printf("Processes: %d, Threads per process: %d, Iterations: %d\n", size, num_threads, iterations);
        printf("Total time: %.6f seconds\n", total_time);
        printf("Average time per barrier: %.6f milliseconds\n", (total_time * 1e3) / iterations);
    }

    gtmp_finalize();
    gtmpi_finalize();
    MPI_Finalize();
    return 0;
}