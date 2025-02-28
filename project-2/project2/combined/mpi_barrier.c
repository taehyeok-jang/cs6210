#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include "mpi_barrier.h"

#include <stdbool.h>

#define WINNER 0
#define LOSER 1
#define CHAMPION 2
#define BYE 3
#define DROPOUT 4
#define UNUSED 5

typedef struct {
    int role;
    int opponent;
    bool flag;
} round_t;

static round_t **rounds; // [0...num_procs-1][0...num_rounds-1]
static int num_rounds; 
int sense;
int rank, num_procs;


void print_rounds(int num_procs, int num_rounds, round_t **rounds) {
    printf("\n%-12s", "Process");
    for (int k = 0; k < num_rounds; k++) {
        printf("| Round %-2d       ", k);
    }
    printf("\n");

    for (int i = 0; i < num_procs; i++) {
        printf("%-12d", i);  // Process ID
        for (int k = 0; k < num_rounds; k++) {
            printf("| Role: %-2d Opp: %-2d ", rounds[i][k].role, rounds[i][k].opponent);
        }
        printf("\n");
    }
}

void gtmpi_init(int num_processes) {

    // printf("gtmpi_init\n");

    num_procs = num_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    num_rounds = ceil(log2(num_procs)) + 1;
    
    // printf("num_procs: %d, rank: %d\n", num_procs, rank);
    // printf("num_rounds: %d\n", num_rounds);

	rounds = (round_t **) malloc (num_processes * sizeof(round_t *));
	for (int i = 0;i < num_procs; i++) {
		rounds[i] = (round_t *) malloc (num_rounds * sizeof (round_t));
	}

    for (int i=0; i<num_procs; i++) {
        for (int k=0; k<num_rounds; k++) {
            rounds[i][k].flag = false;

            // ROLE
            if      (k>0 && (int)fmod(i,pow(2,k))==0 && (i+pow(2,k-1))<num_procs && pow(2,k)<num_procs) {
                rounds[i][k].role = WINNER;
            }
            else if (k>0 && (int)fmod(i,pow(2,k))==0 && (i+pow(2,k-1))>=num_procs) {
                rounds[i][k].role = BYE;
            }
            else if (k>0 && (int)fmod(i,pow(2,k))==pow(2,k-1)) {
                rounds[i][k].role = LOSER;
            }
            else if (k>0 && i==0 && pow(2,k)>=num_procs) {
                rounds[i][k].role = CHAMPION;
            }
            else if (k==0) {
                rounds[i][k].role = DROPOUT;    
            } else {
                rounds[i][k].role = UNUSED;
            }

            // OPPONENT
            if (rounds[i][k].role==WINNER || rounds[i][k].role==CHAMPION) {
                rounds[i][k].opponent = i+pow(2,k-1);
            }
            else if (rounds[i][k].role == LOSER) {
                rounds[i][k].opponent = i-pow(2,k-1);
            }
            else {
                rounds[i][k].opponent = UNUSED;
            }
        }
    }

    // print_rounds(num_procs, num_rounds, rounds);
}

void gtmpi_barrier() {

    if (num_procs == 1) {
        return;
    }

    int _round = 1;
    int loop_exit = 0;
    MPI_Status status; 

    // ARRIVAL
	do {
		switch (rounds[rank][_round].role) {
			case LOSER:
				MPI_Send(&sense, 1, MPI_INT, rounds[rank][_round].opponent, 1, MPI_COMM_WORLD);
				MPI_Recv(&rounds[rank][_round].flag, 1, MPI_INT, rounds[rank][_round].opponent, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				loop_exit = 1;
				break;
			case WINNER:
				MPI_Recv(&rounds[rank][_round].flag, 1, MPI_INT, rounds[rank][_round].opponent, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				break;
            case BYE:
                break; // do nothing 
			case CHAMPION:
				MPI_Recv(&rounds[rank][_round].flag, 1, MPI_INT, rounds[rank][_round].opponent, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				MPI_Send(&sense, 1, MPI_INT, rounds[rank][_round].opponent, 1, MPI_COMM_WORLD);
				loop_exit = 1;
			default:
				break;
			}

		_round += !loop_exit;	

	} while (!loop_exit && _round <= num_rounds);

	loop_exit = 0;

    // WAKEUP
	while (_round > 0 && !loop_exit) {
        _round--;
		switch (rounds[rank][_round].role) {
			case LOSER:
                break; // impossible
            case WINNER:
				MPI_Send(&sense, 1, MPI_INT, rounds[rank][_round].opponent, 1, MPI_COMM_WORLD);
				break;
            case BYE:
                break; // do nothing
            case CHAMPION:
                break; // impossible
			case DROPOUT:
				loop_exit = 1;
			default:
				break;
		}

		if (loop_exit) {
            break;
        }
	}

    sense = !(sense);
}

void gtmpi_finalize() {
    for (int r = 0; r < num_rounds; r++) {
        free(rounds[r]);
    }
    free(rounds);
}