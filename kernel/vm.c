#include "include/param.h"
#include "include/types.h"
#include "include/memlayout.h"
#include "include/elf.h"
#include "include/riscv.h"
#include "include/vm.h"
#include "include/kalloc.h"
#include "include/proc.h"
#include "include/printf.h"
#include "include/string.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S
/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  // printf("kernel_pagetable: %p\n", kernel_pagetable);

  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART_V, UART, PGSIZE, PTE_R | PTE_W);
  
  #ifdef QEMU
  // virtio mmio disk interface
  kvmmap(VIRTIO0_V, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  #endif
  // CLINT
  kvmmap(CLINT_V, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC_V, PLIC, 0x4000, PTE_R | PTE_W);
  kvmmap(PLIC_V + 0x200000, PLIC + 0x200000, 0x4000, PTE_R | PTE_W);

  #ifndef QEMU
  // GPIOHS
  kvmmap(GPIOHS_V, GPIOHS, 0x1000, PTE_R | PTE_W);

  // DMAC
  kvmmap(DMAC_V, DMAC, 0x1000, PTE_R | PTE_W);

  // GPIO
  // kvmmap(GPIO_V, GPIO, 0x1000, PTE_R | PTE_W);

  // SPI_SLAVE
  kvmmap(SPI_SLAVE_V, SPI_SLAVE, 0x1000, PTE_R | PTE_W);

  // FPIOA
  kvmmap(FPIOA_V, FPIOA, 0x1000, PTE_R | PTE_W);

  // SPI0
  kvmmap(SPI0_V, SPI0, 0x1000, PTE_R | PTE_W);

  // SPI1
  kvmmap(SPI1_V, SPI1, 0x1000, PTE_R | PTE_W);

  // SPI2
  kvmmap(SPI2_V, SPI2, 0x1000, PTE_R | PTE_W);

  // SYSCTL
  kvmmap(SYSCTL_V, SYSCTL, 0x1000, PTE_R | PTE_W);
  
  #endif
  
  // map rustsbi
  // kvmmap(RUSTSBI_BASE, RUSTSBI_BASE, KERNBASE - RUSTSBI_BASE, PTE_R | PTE_X);
  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);
  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);
  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  #ifdef DEBUG
  printf("kvminit\n");
  #endif
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  // reg_info();
  sfence_vma();
  #ifdef DEBUG
  printf("kvminithart\n");
  #endif
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == NULL)
        return NULL;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return NULL;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return NULL;
  if((*pte & PTE_V) == 0)
    return NULL;
  if((*pte & PTE_U) == 0)
    return NULL;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  return kwalkaddr(kernel_pagetable, va);
}

uint64
kwalkaddr(pagetable_t kpt, uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kpt, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  
  for(;;){
    if((pte = walk(pagetable, a, 1)) == NULL)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

/**
 * @brief 移除从 va 开始的 npages 个页面的映射。va 必须页对齐
 * @param pagetable 目标用户页表
 * @param va 要取消映射的起始虚拟地址，必须页对齐
 * @param npages 要取消映射的页面数量
 * @param do_free 如果为 1，则释放页面对应的物理内存；如果为 0，则只取消映射
 * @note 在原有基础上进行修改以支持懒加载（Lazy Allocation）
 * @note 如果一个页面因为从未被访问而尚未建立映射，本函数会静默地跳过，而不会触发 panic
 */
void
vmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  // 检查起始地址是否页对齐
  if ((va % PGSIZE) != 0)
    panic("vmunmap: not aligned");

  // 遍历所有需要取消映射的页面地址
  for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
    // 尝试查找该虚拟地址对应的页表项(PTE)，不分配新的页目录（alloc=0）。
    pte = walk(pagetable, a, 0);

    // 懒加载时，mmap 区域直到被访问前，其页表项甚至中间的页目录都可能不存在
    // 所以，如果 walk 返回 NULL，即页表项不存在（没创建），或者 PTE 的有效位为 0（页尚未映射），都是正常的
    if (pte == 0 || (*pte & PTE_V) == 0) {
      // 继续找下一个页面，忽略未映射的页面，不触发 panic
      continue;
    }
    // 页面被映射，但是不是叶子节点，说明页表结构有问题
    if (PTE_FLAGS(*pte) == PTE_V) {
      panic("vmunmap: not a leaf");
    }
    // 如果 do_free 标志被设置，则释放该页表项指向的物理内存
    if (do_free) {
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }

    // 将页表项清零，使其无效，完成取消映射
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == NULL)
    return NULL;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, pagetable_t kpagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  // printf("[uvminit]kalloc: %p\n", mem);
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  mappages(kpagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X);
  memmove(mem, src, sz);
  // for (int i = 0; i < sz; i ++) {
  //   printf("[uvminit]mem: %p, %x\n", mem + i, mem[i]);
  // }
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, pagetable_t kpagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == NULL){
      uvmdealloc(pagetable, kpagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, kpagetable, a, oldsz);
      return 0;
    }
    if (mappages(kpagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R) != 0){
      int npages = (a - oldsz) / PGSIZE;
      vmunmap(pagetable, oldsz, npages + 1, 1);   // plus the page allocated above.
      vmunmap(kpagetable, oldsz, npages, 0);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, pagetable_t kpagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    vmunmap(kpagetable, PGROUNDUP(newsz), npages, 0);
    vmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    vmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

/**
 * @brief 回滚 COW 操作，将 PTE_COW 标记的页恢复为可写状态
 * @param pagetable 页表
 * @param upto 要回滚的结束地址
 * @note 用于在 fork 失败时恢复父进程的页表状态
 */
static void
revert_cow(pagetable_t pagetable, uint64 upto) {
  for (uint64 va = 0; va < upto; va += PGSIZE) {
    pte_t* pte = walk(pagetable, va, 0);
    // 页表项不存在，跳过
    if (pte == 0)
      continue;
    // 页表项无效，跳过
    if ((*pte & PTE_V) == 0)
      continue;
    // 页表项不是 COW 页，跳过
    if ((*pte & PTE_COW) == 0)
      continue;
    // 获取物理页地址
    uint64 pa = PTE2PA(*pte);
    // 如果物理页引用计数为 1，则恢复为可写状态
    if (getref(pa) == 1) {
      // 强制加写权限，移除 COW 位
      uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
      *pte = PA2PTE(pa) | flags;
    }
  }
}

// Given a parent process's page table, copy
// its memory into a child's page table using
// copy-on-write(COW) semantics.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, pagetable_t knew, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i = 0, ki = 0;
  uint flags;

  while (i < sz){
    if((pte = walk(old, i, 0)) == NULL)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    uint64 child_flags = flags;
    int need_cow = 0;

    // 如果父页是可写或已经是 COW，说明需要共享页
    // 已经是 COW 的情况：fork() 之后又有 fork()
    if ((flags & PTE_W) || (flags & PTE_COW)) {
      // 移除 PTE_W，增加 PTE_COW
      child_flags &= ~PTE_W;
      child_flags |= PTE_COW;
      need_cow = 1;
    }

    // 将子用户页表项相应虚拟页映射到父进程对应页表项的物理页，权限为 child_flags
    if (mappages(new, i, PGSIZE, pa, child_flags) != 0) {
      goto err;
    }
    i += PGSIZE;

    // 内核态页表项需要先移除 PTE_U 和 PTE_COW 标志
    // PTE_COW 在内核态页表项中没有意义，发生写异常时是根据用户态页表项的 PTE_COW 位来决定是否触发写时复制
    // 如果触发，会清除同一物理页的所有用户态页面 PTE_COW 位，并新分配物理页然后拷贝数据、更新内核态用户态页表项
    uint64 kchild_flags = child_flags;
    kchild_flags &= ~PTE_U;
    kchild_flags &= ~PTE_COW;
    if (mappages(knew, ki, PGSIZE, pa, kchild_flags) != 0) {
      goto err;
    }
    // 增加物理页引用计数
    incref(pa);

    // 如果需要触发写时复制，则更新父页表项，设置权限与 child_flags 相同
    // 即无 PTE_W，有 PTE_COW
    if (need_cow) {
      *pte = PA2PTE(pa) | child_flags;
    }
    ki += PGSIZE;
  }

  // 刷新 TLB
  sfence_vma();
  return 0;

 err:
  vmunmap(knew, 0, ki / PGSIZE, 0);
  vmunmap(new, 0, i / PGSIZE, 1);
  revert_cow(old, i);
  sfence_vma();
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == NULL)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

/**
 * @brief 对给定虚拟地址所在的页，如果是 COW 页，则根据引用计数决定是直接恢复写权限还是复制一份新页，同时同步更新用户页表和内核页表。
 * @param p 进程
 * @param va 虚拟地址
 * @return 0 成功，-1 失败
 */
int
cow_make_writable(struct proc *p, uint64 va)
{
  pagetable_t pagetable = p->pagetable;
  uint64 va0 = PGROUNDDOWN(va);
  pte_t* pte = walk(pagetable, va0, 0);
  // 页表项不存在或无效，返回错误
  if (pte == 0 || (*pte & PTE_V) == 0)
    return -1;
  // 页表项不是 COW 页，直接返回
  if((*pte & PTE_COW) == 0)
    return 0;

  // 获取物理页地址
  uint64 pa = PTE2PA(*pte);
  int ref = getref(pa);
  // 引用计数小于 1，panic
  if (ref < 1) {
    panic("cow_make_writable");
  }

  // 引用计数为 1，说明是最后一个引用
  if (ref == 1) {
    // 直接恢复 PTE_W 位、移除 PTE_COW 位
    uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
    *pte = PA2PTE(pa) | flags;
    // 类似地更新内核页表
    pte_t* kpte = walk(p->kpagetable, va0, 0);
    if(kpte == 0)
      panic("cow_make_writable kpte");
    uint64 kflags = (PTE_FLAGS(*kpte) | PTE_W) & ~PTE_COW;
    *kpte = PA2PTE(pa) | kflags;
    sfence_vma();
    return 0;
  }

  // 引用计数 > 1，触发写时复制，需要分配新页、复制数据、更新父进程和其内核页表
  char* mem = kalloc();
  if(mem == 0)
    return -1;
  memmove(mem, (char*)pa, PGSIZE);
  // 更新用户页表，设置 PTE_W 位、移除 PTE_COW 位
  uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
  *pte = PA2PTE((uint64)mem) | flags;
  // 类似地更新内核页表
  pte_t* kpte = walk(p->kpagetable, va0, 0);
  if(kpte == 0)
    panic("cow_make_writable kpte");
  uint64 kflags = (PTE_FLAGS(*kpte) | PTE_W) & ~PTE_COW;
  *kpte = PA2PTE((uint64)mem) | kflags;
  sfence_vma();
  kfree((void*)pa);
  return 0;
}

/**
 * @brief 将内核空间的数据拷贝到用户空间
 * @param dstva 目标虚拟地址
 * @param src 源数据
 * @param len 长度
 * @return 0 成功，-1 失败
 */
int
copyout2(uint64 dstva, char *src, uint64 len)
{
  struct proc *p = myproc();
  uint64 sz = p->sz;
  if (dstva + len > sz || dstva >= sz) {
    return -1;
  }
  // 这里原先是直接一个大的 memmove，但是我们现在要处理 COW，所以必须保证每次复制都在一个整页以内
  while (len > 0) {
    uint64 va0 = PGROUNDDOWN(dstva);
    // 处理 COW，确保目标页可写
    if (cow_make_writable(p, va0) < 0){
      return -1;
    }
    // 拷贝数据，初次拷贝可能非整页，而是复制了 [dstva, va0+PGSIZE) 之间的数据
    // 后续拷贝时，n 就是整页大小 PGSIZE
    // 最后一次拷贝时，n = len <= PGSIZE
    uint64 n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)dstva, src, n);
    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

/*
int
copyin2(char *dst, uint64 srcva, uint64 len)
{
  uint64 sz = myproc()->sz;
  if (srcva + len > sz || srcva >= sz) { // bug: 无法处理映射页表
    return -1;
  }
  memmove(dst, (void *)srcva, len);
  return 0;
} */

/**
 * @brief 修复版 copyin2，用于将用户空间的数据拷贝到内核空间
 * @param dst 目标地址（内核空间）
 * @param srcva 源地址（用户空间）
 * @param len 长度
 * @return 0 成功，-1 失败
 * @note 修复了 copyin2 的边界检查问题，即 sz 是堆的上边界（堆顶之后紧接着的第一个无效地址），但是我们可能会从 mmap 的映射区中进行数据读取，从而导致越界，所以这里直接改为 copyin 的简单封装
 */
int
copyin2(char* dst, uint64 srcva, uint64 len) {
  pagetable_t pagetable = myproc()->pagetable;
  return copyin(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

int
copyinstr2(char *dst, uint64 srcva, uint64 max)
{
  int got_null = 0;
  uint64 sz = myproc()->sz;
  while(srcva < sz && max > 0){
    char *p = (char *)srcva;
    if(*p == '\0'){
      *dst = '\0';
      got_null = 1;
      break;
    } else {
      *dst = *p;
    }
    --max;
    srcva++;
    dst++;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// initialize kernel pagetable for each process.
pagetable_t
proc_kpagetable()
{
  pagetable_t kpt = (pagetable_t) kalloc();
  if (kpt == NULL)
    return NULL;
  memmove(kpt, kernel_pagetable, PGSIZE);

  // remap stack and trampoline, because they share the same page table of level 1 and 0
  char *pstack = kalloc();
  if(pstack == NULL)
    goto fail;
  if (mappages(kpt, VKSTACK, PGSIZE, (uint64)pstack, PTE_R | PTE_W) != 0)
    goto fail;
  
  return kpt;

fail:
  kvmfree(kpt, 1);
  return NULL;
}

// only free page table, not physical pages
void
kfreewalk(pagetable_t kpt)
{
  for (int i = 0; i < 512; i++) {
    pte_t pte = kpt[i];
    if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      kfreewalk((pagetable_t) PTE2PA(pte));
      kpt[i] = 0;
    } else if (pte & PTE_V) {
      break;
    }
  }
  kfree((void *) kpt);
}

void
kvmfreeusr(pagetable_t kpt)
{
  pte_t pte;
  for (int i = 0; i < PX(2, MAXUVA); i++) {
    pte = kpt[i];
    if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      kfreewalk((pagetable_t) PTE2PA(pte));
      kpt[i] = 0;
    }
  }
}

void
kvmfree(pagetable_t kpt, int stack_free)
{
  if (stack_free) {
    vmunmap(kpt, VKSTACK, 1, 1);
    pte_t pte = kpt[PX(2, VKSTACK)];
    if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      kfreewalk((pagetable_t) PTE2PA(pte));
    }
  }
  kvmfreeusr(kpt);
  kfree(kpt);
}

void vmprint(pagetable_t pagetable)
{
  const int capacity = 512;
  printf("page table %p\n", pagetable);
  for (pte_t *pte = (pte_t *) pagetable; pte < pagetable + capacity; pte++) {
    if (*pte & PTE_V)
    {
      pagetable_t pt2 = (pagetable_t) PTE2PA(*pte); 
      printf("..%d: pte %p pa %p\n", pte - pagetable, *pte, pt2);

      for (pte_t *pte2 = (pte_t *) pt2; pte2 < pt2 + capacity; pte2++) {
        if (*pte2 & PTE_V)
        {
          pagetable_t pt3 = (pagetable_t) PTE2PA(*pte2);
          printf(".. ..%d: pte %p pa %p\n", pte2 - pt2, *pte2, pt3);

          for (pte_t *pte3 = (pte_t *) pt3; pte3 < pt3 + capacity; pte3++)
            if (*pte3 & PTE_V)
              printf(".. .. ..%d: pte %p pa %p\n", pte3 - pt3, *pte3, PTE2PA(*pte3));
        }
      }
    }
  }
  return;
}


/**
 * @brief 将 VMA 中的数据写回物理内存
 * @param p 进程 PCB 指针
 * @param v 要写回的 VMA 指针
 */
void vma_writeback(struct proc* p, struct vma* v) {
  if (v->valid == 0) {
    return;
  }

  if (!(v->flags & MAP_SHARED) || !(v->prot & PROT_WRITE) || !(v->vm_file)) {
    return;
  }

  if (v->vm_file->writable == 0) {
    return;
  }

  for (uint64 va = v->start; va < v->end; va += PGSIZE) {
    uint64 pa = walkaddr(p->pagetable, va);
    if (pa == 0) {
      continue;
    }
    uint64 file_offset = v->offset + (va - v->start);
    elock(v->vm_file->ep);
    ewrite(v->vm_file->ep, 0, pa, file_offset, PGSIZE);
    eunlock(v->vm_file->ep);
  }
}


/**
 * @brief 释放进程的 VMA
 * @param p 进程 PCB 指针
 */
void vma_free(struct proc* p) {
  for (int i = 0; i < NVMA; i++) {
    struct vma* v = &p->vmas[i];
    if (v->valid) {
      // 将 VMA 中的数据写回物理内存
      // 只有当 VMA 是共享映射，并且是可写，并且是文件映射时，才需要写回物理内存
      vma_writeback(p, v);
      // 取消映射并决定是否释放物理页
      // 如果是共享映射(MAP_SHARED)，则不释放物理内存，
      // 否则（私有或匿名映射）则释放。
      int do_free = (v->flags & MAP_SHARED) ? 0 : 1;
      vmunmap(p->pagetable, v->start, (v->end - v->start) / PGSIZE, do_free);

      // 如果是文件映射，关闭文件
      if (v->vm_file) {
        fileclose(v->vm_file);
        v->vm_file = NULL;
      }

      v->valid = 0;
    }
  }
}


/**
 * @brief 在进程的地址空间中找到一个可用的地址，用于映射文件
 * @param p 进程 PCB 指针
 * @param len 需要映射的长度，是一个 PGSIZE=4096 的整倍数
 * @return 找到的地址，0 表示失败
 * @note 从 MMAPBASE 开始向下搜索，直到找到一个足够大的、不与现有 VMA 或堆栈冲突的空闲区域
 */
uint64 mmap_find_addr(struct proc* p, uint64 len) {
  uint64 addr = MMAPBASE;

  if (len % PGSIZE != 0) {
    return 0;
  }

  while (1) {
    addr -= len;

    // 如果一直找到了和栈顶重叠，则返回失败
    if (addr < p->sz) {
      return 0;
    }

    int conflict = 0;
    for (int i = 0; i < NVMA; i++) {
      struct vma* v = &p->vmas[i];
      if (v->valid && v->start <= addr && v->end >= addr) {
        conflict = 1;
        addr = v->start;
        break;
      }
    }
    if (!conflict) {
      return addr;
    }
  }
}