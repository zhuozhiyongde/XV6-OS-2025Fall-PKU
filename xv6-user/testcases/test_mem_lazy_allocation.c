#include "test.h"

#define SIZE (1 << 14) // 16KB
#define PAGES (SIZE / 4096) // 4KB per page

/*
* Desc:
* We test lazy allocation by allocating a large memory region but only
* actually using a portion of it. We then check the physical page count
* to ensure only the used pages are allocated.
*
* Expected:
* Initially, no extra pages should be allocated. After accessing the first
* few pages, only those pages should be allocated. The judge program will
* check the physical page count at each step to verify lazy allocation.
*/

int main() {
    printf("Testing Lazy Allocation\n");
    
    // Get initial physical page count
    int initial_pages = getpgcnt();
    printf("Initial physical pages: %d\n", initial_pages);
    
    // Allocate a large memory region (should not allocate physical pages yet)
    char *mem = sbrk(SIZE);
    if (mem == (char*)-1) {
        printf("sbrk failed\n");
        exit(1);
    }
    
    int after_alloc_pages = getpgcnt();
    printf("Physical pages after sbrk: %d (should be same as initial)\n", after_alloc_pages);
    
    if (after_alloc_pages != initial_pages) {
        printf("ERROR: Physical pages increased without access!\n");
        exit(1);
    }
    
    // Access first page (should trigger page fault and allocate one page)
    mem[0] = 'A';
    int after_first_access = getpgcnt();
    printf("Physical pages after first access: %d (should be +1)\n", after_first_access);
    
    if (after_first_access != initial_pages + 1) {
        printf("ERROR: Expected %d pages, got %d\n", initial_pages + 1, after_first_access);
        exit(1);
    }
    
    // Access a page in the middle
    mem[SIZE/2] = 'B';
    int after_mid_access = getpgcnt();
    printf("Physical pages after mid access: %d (should be +2)\n", after_mid_access);
    
    if (after_mid_access != initial_pages + 2) {
        printf("ERROR: Expected %d pages, got %d\n", initial_pages + 2, after_mid_access);
        exit(1);
    }
    
    // Access last page
    mem[SIZE-1] = 'C';
    int after_last_access = getpgcnt();
    printf("Physical pages after last access: %d (should be +3)\n", after_last_access);
    
    if (after_last_access != initial_pages + 3) {
        printf("ERROR: Expected %d pages, got %d\n", initial_pages + 3, after_last_access);
        exit(1);
    }
    
    printf("Lazy Allocation Test Completed Successfully\n");
    exit(0);
}