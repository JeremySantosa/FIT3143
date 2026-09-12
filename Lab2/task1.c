#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

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
    int rank, numProcesses;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);

    int n;

    // root process reads n from the command line instead of prompting the user
    if (rank == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <n>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        n = atoi(argv[1]);
    }

    // disseminate n from root to every other process in the communicator
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // allocates memory for array of size n to store prime flags, same idea as
    // task3's primeFlags - but here each process only fills in its own share,
    // leaving every other entry 0, since MPI processes don't share memory
    int *localFlags = calloc(n, sizeof(int));

    // pointer to determine where we should write prime numbers to.
    FILE *outputFile;
    if (rank == 0) {
        if (n < 100) {
            outputFile = stdout;
        }
        else {
            outputFile = fopen("output.txt", "w");
        }
    }

    // wait for every process to arrive here before starting the clock, so
    // the timing is fair regardless of how long processes took to start up
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    /*
    Workload distribution: interleaved (cyclic) partitioning.
    Process 'rank' checks i = 2+rank, 2+rank+numProcesses, 2+rank+2*numProcesses, ...
    A contiguous block split would give the process with the highest range far
    more work, since isPrime's cost grows with sqrt(i) - the largest numbers are
    the most expensive to check. Interleaving spreads cheap small numbers and
    expensive large numbers evenly across every process instead.
    */
    for (int i = 2 + rank; i < n; i += numProcesses) {
        localFlags[i] = isPrime(i);
    }

    // combine every process's flags into one array on root. Since exactly one
    // process ever sets a given index to 1, summing plays the same role that
    // shared memory played for the OpenMP threads in task3 - it reassembles
    // the full, correctly-ordered result.
    int *globalFlags = NULL;
    if (rank == 0) {
        globalFlags = malloc(n * sizeof(int));
    }
    MPI_Reduce(localFlags, globalFlags, n, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        // serially print prime numbers to output file in order
        for (int i = 2; i < n; i++) {
            if (globalFlags[i]) {
                fprintf(outputFile, "%d ", i);
            }
        }
    }

    double end = MPI_Wtime();
    double time_taken = end - start;

    if (rank == 0) {
        // close output file if opened
        if (outputFile != stdout) {
            fclose(outputFile);
        }

        printf("\nExecution time: %f seconds\n", time_taken);

        free(globalFlags);
    }

    // free allocated memory to prevent a memory leak
    free(localFlags);

    MPI_Finalize();
    return 0;
}
