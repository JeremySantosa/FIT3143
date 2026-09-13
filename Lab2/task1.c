#include <math.h>
#include <stdio.h>
#include <mpi.h> // MPI header
#include <stdlib.h> // needed for arrays

/*
Our isPrime function
input: integer n
returns 0 for not prime 1 for prime
*/
int isPrime(int n) {
    // any number less than 2 is not prime
    if (n < 2)
        return 0;

    // getting square root of n for loop
    int limit = sqrt(n);

    //tests all possible diviors from 2 up to sqrt(n)
    for (int i = 2; i <= limit; i++) {
        if (n % i == 0)
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int rank, num_Processes;

    MPI_Init(&argc, &argv); // starts the MPI runtime
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // MPI_COMM_WORLD is a communicator that includes all processes in mpirun. mpi_comm_rank turns a process ID into rank [0 ... num_Processes-1]
    MPI_Comm_size(MPI_COMM_WORLD, &num_Processes); // Writes total process count in num_processes

    int n;
    
    if (rank == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <n>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1); // abort all processes if the usage is incorrect
        }
        n = atoi(argv[1]); // this converts input argument into an integer
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // allocates memory for array of size n to store prime numbers
    int *primeFlags = calloc(n, sizeof(int)); // allocate memory and initialize to zero

    // pointer to determine where we should write prime numbers to. 
    FILE *outputFile;

    // less than 100 print to terminal, otherwise use output file.
    if (rank == 0) {
        if (n < 100) {
            outputFile = stdout;
        }
        else {
            outputFile = fopen("output_t3.txt", "w");
        }
    }

    // record elapsed time before starting
    MPI_Barrier(MPI_COMM_WORLD); // this synchronizes all processes before starting a timer
    double start = MPI_Wtime();

    // check every number from 2 to n. 
    for (int i = 2 + rank; i < n; i += num_Processes) {
        primeFlags[i] = isPrime(i); 
    }

    int *globalFlags = NULL;
    if (rank == 0) {
        globalFlags = malloc(n * sizeof(int)); // allocate memory for global flags array
    }
    MPI_Reduce(primeFlags, globalFlags, n, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // serially print prime numbers to output file in order
    if (rank == 0) {
        for (int i = 2; i < n; i++) {
            if (globalFlags[i]) {
                fprintf(outputFile, "%d ", i);
            }
        }
    }
    // record elapsed time after done
    double end = MPI_Wtime();
    double time_taken = end - start;

    // close output file if opened
    if (rank == 0) {
        if (outputFile != stdout)
        {
            fclose(outputFile);
        }
    }

    if (rank == 0) {
        printf("\nExecution time: %f seconds\n", time_taken);
    }

    // free allocated memory to prevent a memory leak
    free(primeFlags); 
    if (rank == 0) {
        free(globalFlags);
    }

    MPI_Finalize(); // shutdown MPI runtim
    return 0;
}
