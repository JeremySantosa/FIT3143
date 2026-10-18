#include <stdio.h>
#include <mpi.h> // MPI header
#include <stdlib.h> 
#include <time.h>

#define K 6 // Ports per node
#define cycle_seconds 10 // duration of each cycle in seconds

int main(int argc, char *argv[]){
    MPI_Init(&argc, &argv);
    int wrank, wsize, m = argc > 2 ? atoi(argv[1]) : 3, n = argc > 2 ? atoi(argv[2]) : 3;
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);

    if (wsize != m * n + 1) {
        if (wrank == 0) fprintf(stderr, "Need")
    }
}


