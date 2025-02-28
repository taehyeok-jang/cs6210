#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "gtmp.h"

int main(int argc, char** argv)
{
  printf("main...\n");

  int num_threads, num_iter=1000;

  if (argc < 2){
    fprintf(stderr, "Usage: ./harness [NUM_THREADS]\n");
    exit(EXIT_FAILURE);
  }
  num_threads = strtol(argv[1], NULL, 10);

  printf("num_threads: %d, num_iter: %d\n", num_threads, num_iter);

  omp_set_dynamic(0);
  if (omp_get_dynamic())
    printf("Warning: dynamic adjustment of threads has been set\n");

  omp_set_num_threads(num_threads);
  
  gtmp_init(num_threads);

  double start_time, end_time;

  start_time = omp_get_wtime();

  #pragma omp parallel shared(num_threads)
   {
    int i;
    int thread_id = omp_get_thread_num();

    for (i = 0; i < num_iter; i++) {
      printf("at iteration %d\n", i);
        // printf("Thread %d reached barrier at iteration %d\n", thread_id, i);
        gtmp_barrier();
        // printf("Thread %d passed barrier at iteration %d\n", thread_id, i);
        printf("\n");
    }
   }

   end_time = omp_get_wtime();

   double total_time = end_time - start_time;

   printf("Total time: %.6f seconds\n", total_time);
   printf("Average time per barrier: %.6f milliseconds\n", (total_time * 1e3) / num_iter);

   gtmp_finalize();
   return 0;
}
