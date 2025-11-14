// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.


#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"

#define MAX_PHYS_PAGES (PHYSTOP / PGSIZE) // 物理内存页数最大值

void freerange(void *pa_start, void *pa_end);

extern char kernel_end[]; // first address after kernel.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 freepages; // 空闲页数
  uint64 totalpages; // 总分配物理页数（包括空闲页），小于等于 MAX_PHYS_PAGES
  int refcnt[MAX_PHYS_PAGES]; // 引用计数，用于 COW
} kmem;

/**
 * @brief 将物理地址转换为 refcnt 索引
 * @param pa 物理地址，要求必须对齐到 PGSIZE
 * @return refcnt 数组的索引
 */
static inline int
pa2index(uint64 pa)
{
  if(pa % PGSIZE)
    panic("pa2index");
  if(pa >= PHYSTOP)
    panic("pa2index");
  return pa >> PGSHIFT;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.freelist = 0;
  kmem.freepages = 0;
  kmem.totalpages = 0;
  freerange(kernel_end, (void*)PHYSTOP);
  #ifdef DEBUG
  printf("kernel_end: %p, phystop: %p\n", kernel_end, (void*)PHYSTOP);
  printf("kinit\n");
  #endif
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // 现在在初始阶段会计数总分配物理页数，并调用 incref 增加引用计数（对于 COW 来说，这里就是设置为 1）
    kmem.totalpages++;
    incref((uint64)p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < kernel_end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 首先解析物理地址，并转换为 refcnt 索引
  uint64 addr = (uint64)pa;
  int idx = pa2index(addr);

  // 获取锁，并检查引用计数是否大于 0
  // 若引用计数小于 1，则 panic
  // 若引用计数大于 0，则递减引用计数，并返回
  // 若引用计数等于 0，则说明这是最后一次引用，可以真正释放物理页，填充垃圾值，并重新挂回 freelist
  acquire(&kmem.lock);
  if(kmem.refcnt[idx] < 1)
    panic("kfree");
  kmem.refcnt[idx]--;
  if(kmem.refcnt[idx] > 0){
    release(&kmem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.freepages++;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // 这里必然是初次分配，所以减少空闲页数，并设置引用计数为 1
    kmem.freepages--;
    kmem.refcnt[pa2index((uint64)r)] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint64
freemem_amount(void)
{
  return kmem.freepages << PGSHIFT;
}

/**
 * @brief 获取总使用物理页数
 * @return 总使用物理页数
 * @note 总使用物理页数 = 总分配物理页数 - 空闲物理页数
 */
uint64
allocated_pages(void)
{
  uint64 freepages, totalpages;
  acquire(&kmem.lock);
  freepages = kmem.freepages;
  totalpages = kmem.totalpages;
  release(&kmem.lock);
  if (totalpages < freepages) {
    return 0;
  }
  return totalpages - freepages;
}

/**
 * @brief 增加引用计数
 * @param pa 物理地址
 * @note 增加引用计数，用于 COW
 */
void
incref(uint64 pa)
{
  acquire(&kmem.lock);
  int idx = pa2index(pa);
  kmem.refcnt[idx]++;
  release(&kmem.lock);
}

/**
 * @brief 获取引用计数
 * @param pa 物理地址
 * @return 引用计数
 */
int
getref(uint64 pa)
{
  acquire(&kmem.lock);
  int idx = pa2index(pa);
  int cnt = kmem.refcnt[idx];
  release(&kmem.lock);
  return cnt;
}
