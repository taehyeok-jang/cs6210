#include <stdio.h>
#include <omp.h>
#include "omp_barrier.h"
#include "mpi_barrier.h"
#include "combined_barrier.h"
    
void combined_barrier() {
    gtmp_barrier();
    #pragma omp master 
    {
        gtmpi_barrier();
    }
    
    gtmp_barrier();
}
