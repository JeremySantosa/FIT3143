#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// Stores the information required by each worker thread.
typedef struct {
    int start;                  // First number assigned to a thread
    int end;                    // End of range (n-1)
    unsigned char *primeFlags;  // Shared array storing prime result
} ThreadData;                   // data type name


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

    //tests all possible diviors from 2 up to sqrt(2)
    for (int i = 2; i <= limit; i++) {
        if (n % i == 0)
            return 0;
    }
    return 1;
}

/*
is the function run by each POSIX worker thread
(the job given to each thread)
receives its own start and stop range
shares the primeFlag array (thread ranges do not overlap)
returns nothing
*/
void *findPrimes(void *arg) {
    // creating a pointer that directs to Thread data structure. 
    ThreadData *data = (ThreadData *)arg;

    // Each thread searches only inside its assigned range. (e.g 25... 50)
    for (int number = data->start; number < data->end; number++) {   
        /*
        if number is prime add 1 to flag array
        */
        if (isPrime(number)) {
            data->primeFlags[number] = 1;
        }
    }
    return NULL;
}

/*
calculates elapsed time for program to run with multi thread.
gets seconds and nano seconds, converts back into seconds. 
returns time taken in seconds. 
*/
double elapsedTime(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

int main() {
    int n;          // n value
    int numThreads; // number of threads

    // promting user for n value
    printf("Enter number: ");

// error checking 
    if (scanf("%d", &n) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    //ask user how many POSIX threads should be used.
    printf("Enter number of threads: ");

// error checking     
    if (scanf("%d", &numThreads) != 1 || numThreads < 1) {
        printf("Invalid number of threads.\n");
        return 1;
    }

    // total numbers: 2... n - 1 = n-2 
    int totalNumbers = n - 2;

    // prevent creating more threads than n values
    if (numThreads > totalNumbers) {
        numThreads = totalNumbers;
    }

    // creating arrays for thread handles and their associated data (being the n range).
    pthread_t *threads = malloc(numThreads * sizeof(pthread_t));

    ThreadData *threadData = malloc(numThreads * sizeof(ThreadData));

    // creating shared array used to record which numbers are prime (calloc initialises every value to zero)
    unsigned char *primeFlags = calloc(n, sizeof(unsigned char));

    if (threads == NULL || threadData == NULL || primeFlags == NULL) {
        printf("Memory allocation failed.\n");
        free(threads);
        free(threadData);
        free(primeFlags);
        return 1;
    }

    // pointer to determine where we should write prime numbers to. 
    FILE *outputFile;

    // less than 100 print to terminal, otherwise use output file.
    if (n < 100) {
        outputFile = stdout;
    } 
    else {
        outputFile = fopen("task2_output.txt", "w");

        //error prevention
        if (outputFile == NULL) {
            printf("error opening output file.\n");
            free(threads);
            free(threadData);
            free(primeFlags);
            return 1;
        }
    }

    /*
     Static block partitioning:
     Divide the n range as evenly as possiblebetween the worker threads.
     */
    int baseSize = totalNumbers / numThreads;
    int remainder = totalNumbers % numThreads;

    int currentStart = 2;

    struct timespec start;
    struct timespec end;

    // Start measuring elapsed run time.
    clock_gettime(CLOCK_MONOTONIC, &start);


    // create each worker thread.
    for (int i = 0; i < numThreads; i++) {
        int size = baseSize;

        // add leftover numbers among the first threads.
        if (i < remainder) {
            size++;
        }

        // set inital values relative to thread. 
        threadData[i].start = currentStart;
        threadData[i].end = currentStart + size;
        threadData[i].primeFlags = primeFlags;

        // next thread begins immediately after this range.
        currentStart = threadData[i].end;

        int result = pthread_create( &threads[i], NULL, findPrimes, &threadData[i] );
        
        // error preventing
        if (result != 0) {
            printf("Error creating thread %d.\n", i);
            return 1;
        }
    }


    //waiting until every worker thread has completed its work.
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    /*
     assuming all threads finished, we add all prime numbers in assending order using our flag system array. 
     */
    for (int number = 2; number < n; number++) {
        if (primeFlags[number] == 1) {
            fprintf(outputFile, "%d ", number);
        }
    }

    // stop measuring running time.
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken = elapsedTime(start, end);

    // Close the output file if one was opened.
    if (outputFile != stdout) {
        fclose(outputFile);
        // printf( "Prime numbers written to task2_output.txt\n");
    }

    printf( "\nPOSIX Threads execution time: %f seconds\n", time_taken);

    printf("Threads used: %d\n", numThreads);

    // Release memory.
    free(threads);
    free(threadData);
    free(primeFlags);

    return 0;
}
