#include <math.h>
#include <stdio.h>
#include <time.h>

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


int main() {
    int n;
    
    // promting use fon n value
    printf("Enter number: ");
    scanf("%d", &n);

    // pointer to determine where we should write prime numbers to. 
    FILE *outputFile;

    // less than 100 print to terminal, otherwise use output file.
    if (n < 100) {
        outputFile = stdout;
    }
    else {
        outputFile = fopen("output.txt", "w");
    }

    // record cpu time before beginning 
    clock_t start = clock();

    // check every number from 2 to n. 
    for (int i = 2; i < n; i++) {
        if (isPrime(i)) {
            fprintf(outputFile, "%d ", i);
        }
    }

    // record cpu time after done
    clock_t end = clock();

    // convert elapsed clock ticks into seconds. 
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;

    // close output file if opened
    if (outputFile != stdout)
    {
        fclose(outputFile);
    }

    printf("\nExecution time: %f seconds\n", time_taken);

    return 0;
}
