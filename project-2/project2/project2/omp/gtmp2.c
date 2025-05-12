#include <stdio.h>
#include <omp.h>
#include "gtmp.h"

static int num_threads;
static int count __attribute__((aligned(64)));
static int sense __attribute__((aligned(64)));

void gtmp_init(int n) {
    num_threads = n;
    count = n;
    sense = 0;
}

void gtmp_barrier() {
    int local_sense = !sense;

    #pragma omp atomic
    count--;

    if (count == 0) {
        count = num_threads;
        sense = local_sense;
    } else {
        while (sense != local_sense) {
            #pragma omp flush(sense)
        }
    }
}

void gtmp_finalize() {
    // clean-up, if needed
}