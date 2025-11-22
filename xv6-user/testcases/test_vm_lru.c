#include "test.h"

// mmap标志定义
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define MAP_PRIVATE 0x1
#define MAP_ANONYMOUS 0x2

#define TOTAL_PAGES 8
#define PAGE_SIZE 4096

/*
* LRU页面替换算法测试
*/
int main() {
    printf("Testing LRU Page Replacement Algorithm\n");
    sleep(5);
    
    // Set maximum pages in memory to trigger page replacement
    set_max_page_in_mem(4);
    printf("Max pages in memory set to: 4\n");
    
    // Get initial swap count
    int initial_swaps = get_swap_count();
    printf("Initial swap count: %d\n", initial_swaps);
    
    // Allocate memory using mmap (no physical pages allocated initially)
    uint64 mem_addr = mmap(0, TOTAL_PAGES * PAGE_SIZE, 
                           PROT_READ | PROT_WRITE, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (mem_addr == (uint64)-1) {
        printf("mmap failed\n");
        exit(1);
    }
    
    char *mem = (char*)mem_addr;
    printf("Memory mapped at address: 0x%x\n", mem_addr);
    
    // LRU test pattern - exploits temporal locality
    int access_pattern[] = {0, 1, 2, 3, 0, 1, 4, 5, 0, 1, 6, 7, 0, 1, 2, 3};
    int pattern_length = sizeof(access_pattern) / sizeof(access_pattern[0]);
    
    printf("Starting LRU access pattern...\n");
    
    for (int i = 0; i < pattern_length; i++) {
        int page_index = access_pattern[i];
        mem[page_index * PAGE_SIZE] = 'A' + page_index;
        lru_access_notify((uint64)&mem[page_index * PAGE_SIZE]);         // notify kernel the access action
        printf("Accessed page %d\n", page_index);
        sleep(1);
    }
    
    int final_swaps = get_swap_count();
    int total_swaps = final_swaps - initial_swaps;
    printf("Total swaps: %d (expected: 6)\n", total_swaps);

    // Check consistency
    for (int i = 0; i < TOTAL_PAGES; i++) {
        char ch = mem[i * PAGE_SIZE];
        if (ch != 'A' + i) {
            printf("ERROR: Check consistency for page %d failed, expected %c but got %c\n", i, 'A'+i, ch);
            exit(1);
        }
    }
    printf("Check consistency succeeded\n");
    
    // Clean up
    munmap(mem_addr, TOTAL_PAGES * PAGE_SIZE);
    
    if (total_swaps == 6) {
        printf("LRU Test PASSED\n");
        exit(0);
    } else {
        printf("LRU Test FAILED\n");
        exit(1);
    }
}