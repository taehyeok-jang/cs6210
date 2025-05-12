#include <omp.h>
#include <stdio.h>

#define NUM_THREADS 3

int main(int argc, char **argv)
{

  omp_set_num_threads(NUM_THREADS);

  int num_threads = omp_get_num_threads();
  int thread_num = omp_get_thread_num();
  printf("Hello World from thread %d of %d.\n", thread_num, num_threads);

  int i, x = 0;

#pragma omp parallel for shared(x) lastprivate(i) // return the value of i at the last iteration (i.e. 1000)
  for (i = 1; i <= 1000; ++i)
  {
#pragma omp atomic
    x += i;
  }

  printf("sum(1 - %d) = %d\n", i - 1, x);
  return 0;
}

