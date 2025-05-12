#include <stdio.h>
#include <omp.h>
#include "omp_barrier.h"
#include "mpi_barrier.h"
#include "combined_barrier.h"

void combined_init(int num_processes, int num_threads) {

    gtmpi_init(num_processes);
    gtmp_init(num_threads);
}

void combined_barrier(int *sense) {

    int thread_id = omp_get_thread_num();
    int local_sense = *sense;

    gtmp_barrier();

    if (thread_id == 0) {
        gtmpi_barrier();
        *sense = !(*sense);
    }
    else {
        while (local_sense == *sense) {
            // spin
        }
    }
}

void combined_finalize() {
    gtmpi_finalize();
    gtmp_finalize();
}