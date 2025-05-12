#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "gtmpi.h"

int main(int argc, char** argv)
{
  int num_processes, num_rounds = 200;

  MPI_Init(&argc, &argv);
  
  if (argc < 2){
    fprintf(stderr, "Usage: ./harness [NUM_PROCS]\n");
    exit(EXIT_FAILURE);
  }

  num_processes = strtol(argv[1], NULL, 10);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  gtmpi_init(num_processes);

  double start_time, end_time;
  MPI_Barrier(MPI_COMM_WORLD);
  start_time = omp_get_wtime();
  
  int k;
  for(k = 0; k < num_rounds; k++){
    if (rank == 0) {
      printf("at iteration %d\n", k);
    }
    
    // printf("Process %d: Before barrier\n", rank);
    gtmpi_barrier();
    // printf("Process %d: After barrier\n", rank);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  end_time = omp_get_wtime();

  double total_time = end_time - start_time;

  if (rank == 0) {
    printf("Total time: %.6f seconds\n", total_time);
    printf("Average time per barrier: %.6f milliseconds\n", (total_time * 1e3) / num_rounds);
  }

  
  gtmpi_finalize();  

  MPI_Finalize();

  return 0;
}
