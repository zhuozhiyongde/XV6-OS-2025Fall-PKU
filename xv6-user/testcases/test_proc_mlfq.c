#include "test.h"

#define ITERATIONS 1000000
#define LOOP 100

void write_mlfq_completion(int id, int init_prio, int final_prio) {
    char buffer[100]; 
    int pos = 0;
    
    const char prefix[] = "MLFQ Scheduler Process ";
    for (int i = 0; prefix[i] != '\0'; i++) {
        buffer[pos++] = prefix[i];
    }
    
    int temp = id;
    if (temp == 0) {
        buffer[pos++] = '0';
    } else {
        char num_str[10];
        int num_pos = 0;
        while (temp > 0) {
            num_str[num_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int i = num_pos - 1; i >= 0; i--) {
            buffer[pos++] = num_str[i];
        }
    }
    
    const char middle[] = " with initial priority ";
    for (int i = 0; middle[i] != '\0'; i++) {
        buffer[pos++] = middle[i];
    }
    
    temp = init_prio;
    if (temp == 0) {
        buffer[pos++] = '0';
    } else {
        char num_str[10];
        int num_pos = 0;
        while (temp > 0) {
            num_str[num_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int i = num_pos - 1; i >= 0; i--) {
            buffer[pos++] = num_str[i];
        }
    }
    
    const char connector[] = " and final priority ";
    for (int i = 0; connector[i] != '\0'; i++) {
        buffer[pos++] = connector[i];
    }
    
    temp = final_prio;
    if (temp == 0) {
        buffer[pos++] = '0';
    } else {
        char num_str[10];
        int num_pos = 0;
        while (temp > 0) {
            num_str[num_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int i = num_pos - 1; i >= 0; i--) {
            buffer[pos++] = num_str[i];
        }
    }
    
    const char suffix[] = " completed\n";
    for (int i = 0; suffix[i] != '\0'; i++) {
        buffer[pos++] = suffix[i];
    }
    
    write(1, buffer, pos);
}

void cpu_intensive_task(int id, int priority) {
    volatile long long count = 0;
    for(int loop = 0; loop < LOOP*10; loop++) {
        for(int i = 0; i < ITERATIONS; i++) {
            count += (long long)i * (long long) i;
            if(i % (ITERATIONS/2) == 0) {
                // printf("CPU Process %d (prio %d): iteration %d\n", id, priority, i);
            }
        }
    }
    int final_prio = get_priority();
    write_mlfq_completion(id, priority, final_prio);
}

void io_intensive_task(int id, int priority) {
    for(int i = 0; i < 30; i++) {
        // printf("IO Process %d (prio %d): iteration %d\n", id, priority, i);
        sleep(1);  // Simulate I/O operation
    }
    int final_prio = get_priority();
    write_mlfq_completion(id, priority, final_prio);
}

void mixed_task(int id, int priority) {
    volatile long long count = 0;
    for(int i = 0; i < 20; i++) {
        // Do some computation
        for(int j = 0; j < ITERATIONS/10; j++) {
            count += (long long)j * (long long) j;
        }
        // printf("Mixed Process %d (priority %d): iteration %d\n", id, priority, i);
        sleep(1);  // Some I/O
    }
    int final_prio = get_priority();
    write_mlfq_completion(id, priority, final_prio);
}

/*
* Desc:
* We fork five processes with different characteristics and priorities:
* Priority: smaller number = higher priority
* - P1: CPU-intensive, low priority (10) - should be demoted
* - P2: I/O-intensive, high priority (1) - should stay in high queues
* - P3: CPU-intensive, high priority (2) - initially high but may be demoted
* - P4: I/O-intensive, medium priority (5) - shoule be promoted
* - P5: Mixed workload, high priority (3)
*
* Expected:
* The highest priority I/O-bound processes (P2) should finish first, 
* and the lowest priority CPU-bound (P1) should complete last.
* CPU-intensive process P1 & P3 will have an ending priority lower than initial, 
* while I/O-intensive process P4 will have an higher priority than the initial one.
*/

int main() {
    printf("Testing MLFQ Scheduler - Basic\n");

    int pid1, pid2, pid3, pid4, pid5;
    
    // Low priority CPU-bound (may be demoted)
    if((pid1=fork())==0) {
        set_priority(10);   // Lowest priority (largest number)
        cpu_intensive_task(1, 10);
        exit(0);
    }
    
    // Highest priority I/O-bound (should stay in high queues)
    if((pid2=fork())==0) {
        set_priority(1);    // Highest priority (smallest number)
        io_intensive_task(2, 1);
        exit(0);
    }
    
    // High priority CPU-bound (initially high but may be demoted)
    if((pid3=fork())==0) {
        set_priority(2);    // High priority
        cpu_intensive_task(3, 2);
        exit(0);
    }
    
    // Medium priority I/O-bound (initially low but may be promoted)
    if((pid4=fork())==0) {
        set_priority(5);    // Medium priority
        io_intensive_task(4, 5);
        exit(0);
    }
    
    // High priority mixed workload
    if((pid5=fork())==0) {
        set_priority(3);    // High priority
        mixed_task(5, 3);
        exit(0);
    }

    for (int i = 0; i < 5; i++) {
        wait(0);
    }

    printf("MLFQ with Priorities Test Completed\n");
    exit(0);
}