#include "test.h"

#define SIZE (1 << 12)  // 4KB (one page)
#define PGSIZE (1 << 12)

/*
* Desc:
* We test copy-on-write by forking a process and having both parent and child
* read from the same memory. Then we modify the memory in one process and check
* that the physical page count increases appropriately.
*
* Expected:
* After fork, physical page count should remain the same (shared pages).
* After writing to a page in either process, the page should be copied and
* the physical page count should increase by one.
*/

int main() {
    printf("Testing Copy-on-Write\n");
    
    // Get initial physical page count
    int initial_pages = getpgcnt();
    printf("Initial physical pages: %d\n", initial_pages);
    
    // Allocate and initialize a page
    char *mem = sbrk(SIZE);
    if (mem == (char*)-1) {
        printf("sbrk failed\n");
        exit(1);
    }
    
    int write_sum = 0;

    for (int i = 0; i < SIZE; i++) {
        mem[i] = i % 256;
        write_sum += mem[i];
    }
    
    int after_init_pages = getpgcnt();
    printf("Physical pages after initialization: %d (should be +1)\n", after_init_pages);
    
    if (after_init_pages != initial_pages + 1) {
        printf("ERROR: Expected %d pages, got %d\n", initial_pages + 1, after_init_pages);
        exit(1);
    }

    int sz = getprocsz();
    int delta_without_cow = 
        (sz / PGSIZE)   // copy allocated memory pages, in `uvmcopy`
        + 2             // mapping already allocated pages in user page table, in `uvmcopy`
        + 2             // mapping already allocated pages in kernel page table, in `uvmcopy`
        + 1             // user empty page table, in `allocproc`
        + 2             // mapping trampoline, in `proc_pagetable`
        + 1             // trapframe per proc, in `proc_pagetable`
        + 1             // kernel page table, in `proc_kpagetable`
        + 1 + 2         // kernel stack with mapping, in `proc_kpagetable`
        ;
    
    // Fork a process
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }
    
    int after_fork_pages = getpgcnt();
    printf("Physical pages after fork: %d\n", after_fork_pages);
    
    if (after_fork_pages >= after_init_pages + delta_without_cow) {
        printf("ERROR: Page count changed too much from %d to %d after fork without write\n", after_init_pages, after_fork_pages);
    }
    
    if (pid == 0) {
        // Child process
        // Read from the shared page (should not trigger copy)
        int child_before_read = getpgcnt();
        printf("Physical pages before child read: %d\n", child_before_read);

        int sum = 0;
        for (int i = 0; i < SIZE; i++) {
            sum += mem[i];
        }
        printf("Child read sum: %d\n", sum);
        
        if (sum != write_sum) {
            printf("ERROR: Data corruption. Sum should be %d, but got %d\n", write_sum, sum);
            exit(2);
        }
        
        int child_after_read = getpgcnt();
        printf("Physical pages after child read: %d (should be same)\n", child_after_read);
        
        if (child_after_read != child_before_read) {
            printf("ERROR: Page count changed after read\n");
            exit(3);
        }
        
        // Write to the page (should trigger copy)
        mem[0] = 0xFF;
        int child_write_pages = getpgcnt();
        printf("Physical pages after child write: %d (should be increased)\n", child_write_pages);
        
        if (child_write_pages < after_fork_pages + 1) {
            printf("ERROR: Expected at least %d pages, got %d\n", after_fork_pages + 1, child_write_pages);
            exit(1);
        }
        
        exit(0);
    } else {
        // Parent process
        int status ;
        wait(&status);
        printf("wait status:%d\n", status);
        int before_write_pages = getpgcnt();
        
        // Check that parent's page is unchanged
        int sum = 0;
        for (int i = 0; i < SIZE; i++) {
            sum += mem[i];
        }
        printf("Parent read sum: %d\n", sum);

        if (sum != write_sum) {
            printf("ERROR: Data corruption. Sum should be %d, but got %d\n", write_sum, sum);
            exit(1);
        }
        
        int after_read_pages = getpgcnt();
        printf("Physical pages after child exit: %d (should be same as before read)\n", after_read_pages);
        
        if (after_read_pages != before_write_pages) {
            printf("ERROR: Expected %d pages, got %d\n", before_write_pages, after_read_pages);
            exit(1);
        }
        
        // Parent writes to the page (should not trigger copy as child is gone)
        mem[0] = 0xAA;
        int parent_write_pages = getpgcnt();
        printf("Physical pages after parent write: %d (should be same)\n", parent_write_pages);
        
        if (parent_write_pages != after_read_pages) {
            printf("ERROR: Page count changed after parent write\n");
            exit(1);
        }
    }
    
    printf("Copy-on-Write Test Completed Successfully\n");
    exit(0);
}