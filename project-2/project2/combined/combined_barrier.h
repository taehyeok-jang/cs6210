#ifndef COMBINED_H
#define COMBINED_H

void combined_init(int num_processes, int num_threads);
void combined_barrier(int *sense);
void combined_finalize();

#endif
