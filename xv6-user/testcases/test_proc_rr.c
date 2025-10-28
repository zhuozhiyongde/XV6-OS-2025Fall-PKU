#include "test.h"

#define ITERATIONS 1000000
#define LOOP 100

void task(int id) {
    volatile long long count = 0;
    for(int loop = 0; loop < LOOP; loop++) {
        for(int i = 0; i < ITERATIONS; i++) {
            count += (long long)i * (long long) i;
            if(i % (ITERATIONS/2) == 0) {
                // printf("RR Scheduler Process %d: iteration %d\n", id, i);
            }
        }
    }

    char buffer[50];
    int pos = 0;
    const char* parts[] = {"RR Scheduler Process ", "0", " completed\n"};
    for (int i = 0; parts[0][i] != '\0'; i++) {
        buffer[pos++] = parts[0][i];
    }
    buffer[pos++] = '0' + id;
    for (int i = 0; parts[2][i] != '\0'; i++) {
        buffer[pos++] = parts[2][i];
    }

    write(1, buffer, pos);
}
    

/*
* Desc:
* We fork three processes, and set timeslice 1 to P1, 2 to P2, 3 to P3.
* The three processes just do the same work, and will last a long enough time 
* to encounter several timer interrupts.
*
* Expected:
* P3 will finish the job first, then P2, and P1 is the last.
* The judge program will check the appearance and order of `Process {\d} completed`
* to make sure you have implemented the Round-Robin algorithm with given timeslice.
*/

int main() {
    printf("Testing RR Scheduler - Basic\n");

    int pid1, pid2, pid3;
    if((pid1=fork())==0) {
        set_timeslice(1);
        task(1);
        exit(0);
    }
    if((pid2=fork())==0) {
        set_timeslice(2);
        task(2);
        exit(0);
    }
    if((pid3=fork())==0) {
        set_timeslice(3);
        task(3);
        exit(0);
    }
    wait(0);
    wait(0);
    wait(0);

    printf("RR Basic Test Completed\n");
    exit(0);
}