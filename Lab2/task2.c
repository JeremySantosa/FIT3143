#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>

/*
Code Comments were created with help of Google Gemini to ensure readability and understandability for accessor. 
*/

/*
Our isPrime function
input: integer n
returns 0 for not prime, 1 for prime
*/
int isPrime(int n) {

    // Any number less than 2 is not prime
    if (n < 2)
        return 0;

    // Only need to test divisors up to sqrt(n)
    int limit = sqrt(n);

    for (int i = 2; i <= limit; i++) {
        if (n % i == 0)
            return 0;
    }

    return 1;
}


int main(int argc, char *argv[]) {

    int rank;
    int numProcesses;

    /*
     * Because this is a hybrid MPI + OpenMP program,
     * initialise MPI with thread support.
     *
     * MPI_THREAD_FUNNELED means:
     * OpenMP threads may exist, but only the main thread
     * of each MPI process will make MPI calls.
     */
    int provided;

    MPI_Init_thread(
        &argc,
        &argv,
        MPI_THREAD_FUNNELED,
        &provided
    );

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);


    /*
     * Check that the MPI implementation provides
     * the thread support required by our program.
     */
    if (provided < MPI_THREAD_FUNNELED) {

        if (rank == 0) {
            fprintf(stderr,
                    "MPI does not provide required thread support.\n");
        }

        MPI_Abort(MPI_COMM_WORLD, 1);
    }


    int n;
    int numThreads;


    /*
     * Root process reads:
     *
     * argv[1] = upper limit n
     * argv[2] = OpenMP threads per MPI process
     */
    if (rank == 0) {

        if (argc < 3) {

            fprintf(stderr,
                    "Usage: %s <n> <threads_per_process>\n",
                    argv[0]);

            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        n = atoi(argv[1]);
        numThreads = atoi(argv[2]);


        // Validate input
        if (n < 2 || numThreads < 1) {

            fprintf(stderr,
                    "n must be >= 2 and threads must be >= 1.\n");

            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }


    /*
     * Send n from root to all MPI processes.
     */
    MPI_Bcast(
        &n,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );


    /*
     * Send the requested number of OpenMP threads
     * to every MPI process.
     */
    MPI_Bcast(
        &numThreads,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );


    /*
     * Ensure OpenMP uses the number of threads
     * requested by the user.
     */
    omp_set_dynamic(0);
    omp_set_num_threads(numThreads);


    /*
     * Every MPI process owns its own localFlags array.
     *
     * MPI processes do NOT share memory.
     *
     * Each rank only changes the entries corresponding
     * to the numbers assigned to that rank.
     */
    int *localFlags = calloc(n, sizeof(int));

    if (localFlags == NULL) {

        fprintf(stderr,
                "Rank %d failed to allocate localFlags.\n",
                rank);

        MPI_Abort(MPI_COMM_WORLD, 1);
    }


    /*
     * Only root requires the global result array.
     */
    int *globalFlags = NULL;

    if (rank == 0) {

        globalFlags = calloc(n, sizeof(int));

        if (globalFlags == NULL) {

            fprintf(stderr,
                    "Root failed to allocate globalFlags.\n");

            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }


    /*
     * Output is only handled by the root process.
     */
    FILE *outputFile = NULL;

    if (rank == 0) {

        if (n < 100) {
            outputFile = stdout;
        }
        else {

            outputFile = fopen("task2_output.txt", "w");

            if (outputFile == NULL) {

                fprintf(stderr,
                        "Error opening output file.\n");

                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }


    /*
     * Make every MPI process wait here so that they
     * begin the measured part of the program together.
     */
    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();


    /*
     * HYBRID WORKLOAD DISTRIBUTION
     *
     * Level 1:
     * MPI performs cyclic/interleaved partitioning.
     *
     * Example with 4 MPI processes:
     *
     * Rank 0: 2, 6, 10, 14, ...
     * Rank 1: 3, 7, 11, 15, ...
     * Rank 2: 4, 8, 12, 16, ...
     * Rank 3: 5, 9, 13, 17, ...
     *
     * Level 2:
     * OpenMP divides each rank's iterations
     * between threads within that MPI process.
     */
#pragma omp parallel for schedule(static)
    for (int i = 2 + rank;
         i < n;
         i += numProcesses) {

        localFlags[i] = isPrime(i);
    }


    /*
     * Every MPI process now has only part of the result.
     *
     * MPI_Reduce combines the arrays using MPI_SUM.
     *
     * Because only one MPI rank owns each candidate,
     * each prime index can only contain one value of 1.
     */
    MPI_Reduce(
        localFlags,
        globalFlags,
        n,
        MPI_INT,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    /*
     * Root scans the complete array from low to high.
     *
     * This automatically produces sorted output,
     * so no additional sorting operation is needed.
     */
    if (rank == 0) {

        for (int i = 2; i < n; i++) {

            if (globalFlags[i]) {
                fprintf(outputFile, "%d ", i);
            }
        }

    /*
     * Make sure buffered output is actually written
     * before stopping the timer.
     */
    fflush(outputFile);
    }


    /*
    * All MPI processes wait until the root has completed
    * computation, reduction and file output.
    */
    MPI_Barrier(MPI_COMM_WORLD);

    double end = MPI_Wtime();

    double timeTaken = end - start;


    /*
     * Only root reports timing information.
     */
    if (rank == 0) {

        if (outputFile != stdout) {
            fclose(outputFile);
        }

        printf("\nHybrid MPI + OpenMP execution time: %f seconds\n",
               timeTaken);

        printf("MPI processes: %d\n",
               numProcesses);

        printf("OpenMP threads per process: %d\n",
               numThreads);

        printf("Total OpenMP threads created: %d\n",
               numProcesses * numThreads);

        free(globalFlags);
    }


    free(localFlags);

    MPI_Finalize();

    return 0;
}
