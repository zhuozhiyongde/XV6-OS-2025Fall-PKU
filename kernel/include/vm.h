#ifndef __VM_H 
#define __VM_H 

#include "types.h"
#include "riscv.h"

// 前向声明
struct proc;

#ifdef ALGO
#define VMA_MAX_TRACKED_PAGES 128

enum mmap_page_state {
  VMA_PAGE_UNUSED = 0,
  VMA_PAGE_INMEM,
  VMA_PAGE_SWAPPED,
};

struct mmap_vpage {
  int state;
  uint64 load_time;
  uint64 last_access;
  char *swap_data;
};

#endif

void            kvminit(void);
void            kvminithart(void);
uint64          kvmpa(uint64);
void            kvmmap(uint64, uint64, uint64, int);
int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     uvmcreate(void);
// void            uvminit(pagetable_t, uchar *, uint);
void            uvminit(pagetable_t, pagetable_t, uchar *, uint);
uint64          uvmalloc(pagetable_t, pagetable_t, uint64, uint64);
uint64          uvmdealloc(pagetable_t, pagetable_t, uint64, uint64);
// int             uvmcopy(pagetable_t, pagetable_t, uint64);
int             uvmcopy(pagetable_t, pagetable_t, pagetable_t, uint64);
void            uvmfree(pagetable_t, uint64);
// void            uvmunmap(pagetable_t, uint64, uint64, int);
void            vmunmap(pagetable_t, uint64, uint64, int);
void            uvmclear(pagetable_t, uint64);
uint64          walkaddr(pagetable_t, uint64);
pte_t*          walk(pagetable_t, uint64, int);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
pagetable_t     proc_kpagetable(void);
void            kvmfreeusr(pagetable_t kpt);
void            kvmfree(pagetable_t kpagetable, int stack_free);
uint64          kwalkaddr(pagetable_t pagetable, uint64 va);
int             copyout2(uint64 dstva, char *src, uint64 len);
int             copyin2(char *dst, uint64 srcva, uint64 len);
int             copyinstr2(char *dst, uint64 srcva, uint64 max);
void            vmprint(pagetable_t pagetable);
int             cow_make_writable(struct proc *p, uint64 va);

// vma （virtual memory area） 相关函数和宏定义
#define NVMA 16

#define PROT_READ       (1 << 0)
#define PROT_WRITE      (1 << 1)
#define PROT_EXEC       (1 << 2)

#define MAP_PRIVATE     0x01
#define MAP_ANONYMOUS   0x02
#define MAP_SHARED      0x04
#define MAP_FIXED       0x08

struct vma {
    int valid;              // 是否有效
    uint64 start;           // 起始地址
    uint64 end;             // 结束地址
    int prot;               // 内存区域的访问权限，PROT_*
    int flags;              // 描述 VMA 行为的标志位，MAP_*
    struct file* vm_file;   // 文件指针，如果是文件映射，指向对应的 file 结构体；如果是匿名映射，则为 NULL
    uint64 offset;          // 文件偏移量，只有文件映射时有效

    #ifdef ALGO
    int page_count;         // VMA 覆盖的页数量
    struct mmap_vpage *pages; // 指向跟踪 mmap 页信息的数组
    #endif
};

void vma_writeback(struct proc* p, struct vma* v);
void vma_free(struct proc* p);
uint64 mmap_find_addr(struct proc* p, uint64 len);

#ifdef ALGO
void vma_reset_pages(struct proc* p, struct vma* v);
#endif

#endif 
