#include <math.h>
#include <stdio.h>
#include <time.h>
#include <omp.h> // openMP header
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
    int n;
    
    // promting use fon n value
    printf("Enter number: ");
    scanf("%d", &n);

    // allocates memory for array of size n to store prime numbers
    int *primeFlags = malloc(n * sizeof(int)); 

    // pointer to determine where we should write prime numbers to. 
    FILE *outputFile;

    // less than 100 print to terminal, otherwise use output file.
    if (n < 100) {
        outputFile = stdout;
    }
    else {
        outputFile = fopen("output_t3.txt", "w");
    }

    struct timespec start;
    struct timespec end;

    // record elapsed time before beginning 
    clock_gettime(CLOCK_MONOTONIC, &start);

    // this parallelizes the for loop into multiple threads
    #pragma omp parallel for schedule(static)
    // check every number from 2 to n. 
    for (int i = 2; i < n; i++) {
        // printf("Thread %d is checking number %d\n", omp_get_thread_num(), i);
        // store result of isPrime in the primes array
        primeFlags[i] = isPrime(i); 
    }

    // serially print prime numbers to output file in order
    for (int i = 2; i < n; i++) {
        if (primeFlags[i]) {
            fprintf(outputFile, "%d ", i);
        }
    }

    // record elapsed time after done
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken = elapsedTime(start, end);

    // close output file if opened
    if (outputFile != stdout)
    {
        fclose(outputFile);
    }

    printf("\nExecution time: %f seconds\n", time_taken);

    // free allocated memory to prevent a memory leak
    free(primeFlags); 
    return 0;
}
