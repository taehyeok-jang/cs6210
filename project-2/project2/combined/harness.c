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

    int num_threads = 4;
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    }
    omp_set_num_threads(num_threads);

    gtmpi_init(size);
    gtmp_init(num_threads);

    // Performance + Sanity Test
    int iterations = 2;  // 반복 횟수
    double start_time, end_time;
    int failed = 0;  // Sanity test 실패 여부

    // Warm-up phase
    #pragma omp parallel
    {
        for (int i = 0; i < 10; i++) {
            combined_barrier();
        }
    }

    // 모든 프로세스 동기화 후 시간 측정 시작
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = omp_get_wtime();

    // Performance test loop with sanity check
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int local_flag = 0;

        for (int i = 0; i < iterations; i++) {
            local_flag = 0;  // 각 반복마다 초기화
            #pragma omp atomic
            local_flag += 1;  // Barrier 전 증가

            printf("Before barrier: Rank %d, Thread %d, local_flag = %d\n", rank, tid, local_flag);
            combined_barrier();  // Barrier 호출

            printf("After barrier: Rank %d, Thread %d, local_flag = %d\n", rank, tid, local_flag);

            // Barrier 후 local_flag 검증 (변경되지 않아야 함)
            if (local_flag != 1) {
                #pragma omp critical
                printf("❌ Sanity Test Failed at iteration %d: Rank %d, Thread %d, local_flag = %d (expected 1)\n",
                       i, rank, tid, local_flag);
                failed = 1;
            }
        }
    }

    // Global flag 검증 (각 프로세스에서 스레드 수와 일치해야 함)
    int local_sum = num_threads;  // 각 프로세스의 스레드 수
    int global_flag = 0;
    MPI_Reduce(&local_sum, &global_flag, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // 시간 측정 종료
    MPI_Barrier(MPI_COMM_WORLD);
    end_time = omp_get_wtime();

    // 결과 출력
    if (rank == 0) {
        int expected = size * num_threads;
        if (global_flag != expected || failed) {
            printf("❌ Sanity Test Failed! global_flag = %d (expected %d), failed = %d\n",
                   global_flag, expected, failed);
        } else {
            printf("✅ Sanity Test Passed! global_flag = %d\n", global_flag);
        }

        double total_time = end_time - start_time;
        printf("Combined Barrier Performance:\n");
        printf("Processes: %d, Threads per process: %d, Iterations: %d\n", size, num_threads, iterations);
        printf("Total time: %.6f seconds\n", total_time);
        printf("Average time per barrier: %.6f microseconds\n", (total_time * 1e6) / iterations);
    }

    gtmp_finalize();
    gtmpi_finalize();
    MPI_Finalize();
    return 0;
}