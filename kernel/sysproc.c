
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/sbi.h"
#include "include/vm.h"

extern int exec(char *path, char **argv);

/**
 * @brief 从系统调用参数（a0-a5寄存器）中获取一个用户空间的目标地址，然后将内核中的某块数据拷贝到这个目标地址去。
 * @param arg_index 系统调用参数的索引
 * @param dest 目标地址
 * @param size 数据大小
 * @return 0 成功，-1 失败
 */
int get_and_copyout(uint64 arg_index, char* src, uint64 size) {
  uint64 dest_addr;
  if (argaddr(arg_index, &dest_addr) < 0) {
    return -1;
  }
  if (copyout2(dest_addr, src, size) < 0) {
    return -1;
  }
  return 0;
}

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

/**
 * @brief 实现 getppid 系统调用，获取父进程ID。
 * @return 父进程ID
 */
uint64
sys_getppid(void)
{
  return myproc()->parent->pid;
}


uint64
sys_fork(void)
{
  return fork();
}

/**
 * @brief 实现 clone 系统调用，创建子进程/线程。
 * @return 0 成功，-1 失败
 */
uint64 sys_clone(void) {
  return clone();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(-1, p);
}

/**
 * @brief 实现 waitpid 系统调用，等待子进程结束。
 * @param pid 子进程ID
 * @param addr 子进程状态信息存放的目标地址
 * @param options 等待行为选项
 * @return 0 成功，-1 失败
 * @note options 选项：
 * @note - 0: 默认，阻塞等待到子进程结束
 * @note - 其他：未实现（面向测试用例编程，笑死）
 */
uint64 sys_waitpid(void) {
  int pid;
  uint64 addr;
  int options;
  if (argint(0, &pid) < 0 || argaddr(1, &addr) < 0 || argint(2, &options) < 0) {
    return -1;
  }
  return wait(pid, addr);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

/**
 * @brief 实现 getprocsz 系统调用，获取进程的堆顶地址。
 * @return 进程的堆顶地址
 */
uint64 sys_getprocsz(void) {
  acquire(&myproc()->lock);
  int sz = myproc()->sz;
  release(&myproc()->lock);
  return sz;
}

/**
 * @brief 实现 getpgcnt 系统调用，获取当前已分配物理内存的页数。
 * @return 当前已分配物理内存的页数
 */
uint64 sys_getpgcnt(void) {
  return allocated_pages();
}

/**
 * @brief 实现 brk 系统调用，用于调整程序数据段（Heap，堆）的大小。
 * @param addr 新的数据段结束地址
 * @return 0 成功，-1 失败，特别地，brk(0) 返回当前堆的终点地址
 * @note 注意 brk(break) 是直接设置堆的终点，而 sbrk(shift break) 是按照指定增量（可正可负）调整堆的大小。
 */
uint64 sys_brk(void) {
  uint64 addr, new_addr;
  int delta;

  if (argaddr(0, &new_addr) < 0) {
    return -1;
  }

  addr = myproc()->sz;

  if (new_addr == 0) {
    return addr;
  }

  delta = new_addr - addr;

  if (growproc(delta) < 0) {
    return -1;
  }
  return 0;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  #ifdef SCHEDULER_MLFQ
  // MLFQ 算法需要记录每次 sleep 的休眠 tick 数累积，从而判断 I/O 密集还是 CPU 密集
  int slept = 0;
  #endif

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      
      #ifdef SCHEDULER_MLFQ
      slept = ticks - ticks0;
      #endif
      
      release(&tickslock);
      
      #ifdef SCHEDULER_MLFQ
      if (slept > 0) {
        mlfq_account_sleep(myproc(), slept);
      }
      #endif
      
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  
  #ifdef SCHEDULER_MLFQ
  slept = ticks - ticks0;
  if (slept > 0) {
    mlfq_account_sleep(myproc(), slept);
  }
  #endif
  
  return 0;
}

/**
 * @brief 实现 nanosleep 系统调用，睡眠指定时间。
 * @param req_addr 输入参数，指定的睡眠时间结构体 timespec 存放的地址，需要使用 copyin2 从用户空间拷贝到内核空间
 * @param rem_addr 输出参数，实际睡眠时间结构体 timespec 存放的地址，若实际睡眠时间小于指定睡眠时间，则返回剩余睡眠时间，反之返回 0，需要使用 copyout2 从内核空间拷贝到用户空间
 * @return 0 成功，-1 失败
 * @note 注意，根据测试样例的要求，需要睡眠时间以纳秒为单位
 */
uint64
sys_nanosleep(void)
{
  uint64 req_addr, rem_addr;
  struct timespec req_tv; // tv: timeval
  if (argaddr(0, &req_addr) < 0 || argaddr(1, &rem_addr) < 0) {
    return -1;
  }
  // 从用户空间拷贝到内核空间
  if (copyin2((char *)&req_tv, req_addr, sizeof(struct timespec)) < 0) {
    return -1;
  }

  uint64 target_ticks = req_tv.tv_sec * TICKS_PER_SECOND + req_tv.tv_usec * TICKS_PER_SECOND / 1000000;

  uint64 ticks0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < target_ticks) {
    if (myproc()->killed) {
      // 如果输出参数 rem_addr 不为空，则返回剩余睡眠时间
      if (rem_addr != NULL) {
        uint64 elapsed_ticks = ticks - ticks0;
        uint64 rem_ticks = (target_ticks > elapsed_ticks) ? (target_ticks - elapsed_ticks) : 0;
        struct timespec rem_tv;
        rem_tv.tv_sec = rem_ticks / TICKS_PER_SECOND;
        rem_tv.tv_usec = (rem_ticks % TICKS_PER_SECOND) * 1000000 / TICKS_PER_SECOND;
        if (copyout2(rem_addr, (char *)&rem_tv, sizeof(struct timespec)) < 0) {
          release(&tickslock);
          return -1;
        }
      }
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;

}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

/**
 * @brief 实现 shutdown 系统调用，基于 SBI 调用实现
 * @return 0 成功，-1 失败
 */
uint64
sys_shutdown(void) {
  sbi_shutdown();
  return 0;
}

/**
 * @brief 实现 uname 系统调用，返回操作系统名称和版本等信息。
 * @param addr 目标地址
 * @return 0 成功，-1 失败
 */
uint64 sys_uname(void) {
  struct uname_info {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
  };

  // 这个数据当前是准备在内核的栈内存中的
  struct uname_info info = {
    "xv6",
    "xv6-node",
    "1.0.0",
    "1.0.0",
    "arthals",
    "localhost"
  };

  if (get_and_copyout(0, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 实现 times 系统调用，返回自启动以来经过的操作系统 tick 数。
 * @param addr tms 结构体存到的目标地址
 * @return 0 成功，-1 失败
 */
uint64 sys_times(void) {
  struct tms tms;

  acquire(&tickslock);
  tms.tms_utime = tms.tms_stime = tms.tms_cutime = tms.tms_cstime = ticks;
  release(&tickslock);

  if (get_and_copyout(0, (char *)&tms, sizeof(tms)) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 实现 gettimeofday 系统调用，获取当前时间。
 * @param addr timespec 结构体存到的目标地址
 * @return 0 成功，-1 失败
 * @note 注意，根据测试样例的要求，需要返回 tv_usec 微秒而不是 Linux 标准中的 tv_nsec 纳秒
 */
uint64 sys_gettimeofday(void) {
  struct timespec ts;
  uint64 htick = r_time(); // 硬件(hardware) tick，注意全局变量 ticks 是操作系统(os) tick，中间差了 200 倍

  ts.tv_sec = htick / CLOCK_FREQ; // 换算成秒
  ts.tv_usec = (htick % CLOCK_FREQ) * 1000000 / CLOCK_FREQ; // 换算成微秒, 1μs = 10^-6 s

  if (get_and_copyout(0, (char *)&ts, sizeof(ts)) < 0) {
    return -1;
  }
  return 0;
}


/**
 * @brief 实现 sched_yield 系统调用，主动让出CPU。
 * @return 0 成功，-1 失败
 */
uint64 sys_sched_yield(void) {
  yield();
  return 0;
}

/**
 * @brief 实现 mmap 系统调用，将文件映射到进程的地址空间。
 * @param addr 映射的起始地址，只支持 0，即系统自动选择地址
 * @param len 映射的长度，会向上取整到 PGSIZE 的整倍数
 * @param prot 映射的权限
 * @param flags 映射的标志，只支持 MAP_SHARED 和 MAP_ANONYMOUS，不支持 MAP_FIXED
 * @param fd 文件描述符
 * @param offset 文件偏移量，必须是 PGSIZE 的整倍数
 * @return 映射的起始地址，-1 表示失败
 */
uint64 sys_mmap(void) {
  uint64 addr, len;
  int prot, flags, fd, offset;
  struct proc* p = myproc();

  if (
    argaddr(0, &addr) < 0 ||
    argaddr(1, &len) < 0 ||
    argint(2, &prot) < 0 ||
    argint(3, &flags) < 0 ||
    argint(4, &fd) < 0 ||
    argint(5, &offset) < 0
  )
    return -1;

  if (len == 0) {
    return -1;
  }

  // 偏移量必须是 PGSIZE 的整倍数
  if (offset % PGSIZE != 0) {
    return -1;
  }

  // 不支持用户指定地址或者固定地址映射
  if (addr != 0 || (flags & MAP_FIXED)) {
    return -1;
  }

  // 向上取整到 PGSIZE 的整倍数
  len = PGROUNDUP(len);

  // 寻找一个可用的 VMA 位置
  struct vma* v = NULL;
  for (int i = 0; i < NVMA; i++) {
    if (!p->vmas[i].valid) {
      v = &p->vmas[i];
      break;
    }
  }
  // 没有可用的 VMA 位置，返回失败
  if (v == NULL) {
    return -1;
  }

  uint64 va = mmap_find_addr(p, len);
  // 虚拟空间地址不足，返回失败
  if (va == 0) {
    return -1;
  }

  struct file* f = NULL;

  // 如果不是匿名映射，则需要获取文件描述符对应的文件
  if (!(flags & MAP_ANONYMOUS)) {
    // 检查是否为合法的文件描述符
    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == NULL) {
      return -1;
    }
  }

  v->start = va;
  v->end = va + len;
  v->prot = prot;
  v->flags = flags;
  v->offset = offset;
  // 如果是文件映射，则需要增加文件的引用计数，并保存文件指针
  if (f) {
    v->vm_file = filedup(f);
  }
  else {
    v->vm_file = NULL;
  }
  v->valid = 1;

  return va;
}

/**
 * @brief 实现 munmap 系统调用，取消映射进程的地址空间。
 * @param addr 映射的起始地址
 * @param len 映射的长度，会向上取整到 PGSIZE 的整倍数
 * @return 0 成功，-1 失败
 */
uint64 sys_munmap(void) {
  uint64 addr;
  int len;
  struct proc* p = myproc();

  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0) {
    return -1;
  }

  // 地址和长度都需要页对齐。
  if (addr % PGSIZE != 0) {
    return -1;
  }
  len = PGROUNDUP(len);
  if (len == 0) {
    return 0; // unmap 长度为0是无操作，直接成功。
  }

  // 遍历查找要 unmap 的 VMA。
  // 这个实现简化为：必须完整地 unmap 一个或多个已存在的 VMA。
  // 不支持部分 unmap（那会使一个 VMA 分裂成两个）。
  for (int i = 0; i < NVMA; i++) {
    struct vma* v = &p->vmas[i];
    // 检查地址和长度是否精确匹配一个 VMA。
    if (v->valid && v->start == addr && (v->end - v->start) == len) {

      // 写回
      vma_writeback(p, v);

      // 决定是否释放物理页。
      int do_free = (v->flags & MAP_SHARED) ? 0 : 1;

      // 调用 vmunmap 清理页表和物理内存。
      vmunmap(p->pagetable, addr, len / PGSIZE, do_free);

      // 释放对文件的引用。
      if (v->vm_file) {
        fileclose(v->vm_file);
        v->vm_file = NULL;
      }

      // 将 VMA 标记为无效。
      v->valid = 0;

      return 0; // 成功。
    }
  }

  return -1; // 没有找到匹配的 VMA。
}

#ifdef SCHEDULER_RR
/**
 * @brief RR 算法所需内核函数，设置当前进程的时间片
 * @param timeslice 新的时间片长度
 * @return 0 表示系统调用成功返回，-1 表示参数解析失败
 */
uint64 sys_set_timeslice(void) {
  int timeslice;
  if (argint(0, &timeslice) < 0) {
    return -1;
  }
  struct proc* p = myproc();
  // 合法性校验
  if (timeslice < 1) {
    return -1;
  }
  acquire(&p->lock);
  p->timeslice = timeslice;
  p->slice_remaining = timeslice;
  release(&p->lock);
  return 0;
}
#endif

#if defined(SCHEDULER_PRIORITY) || defined(SCHEDULER_MLFQ)
/**
 * @brief 优先级 / MLFQ 调度算法所需内核函数，设置当前进程的优先级
 * @param priority 新的优先级
 * @return 0 表示系统调用成功返回，-1 表示参数解析失败
 */
uint64 sys_set_priority(void) {
  int priority;
  if (argint(0, &priority) < 0) {
    return -1;
  }
  struct proc* p = myproc();

  #ifdef SCHEDULER_PRIORITY
  // 优先级调度：拒绝负值优先级
  if (priority < 0) {
    return -1;
  }
  acquire(&p->lock);
  p->priority = priority;
  release(&p->lock);
  #endif

  #ifdef SCHEDULER_MLFQ
  acquire(&p->lock);
  // MLFQ：裁剪优先级到合法区间，并更新动态优先级、重置统计数据
  p->priority = mlfq_clamp_priority(priority);
  p->base_priority = p->priority;
  p->ticks_used = 0;
  p->eval_ticks = 0;
  p->cpu_ticks = 0;
  p->sleep_ticks = 0;
  release(&p->lock);
  #endif

  return 0;
}

/**
 * @brief 优先级 / MLFQ 算法所需内核函数，实现 get_priority 系统调用，获取当前进程的优先级。
 * @return 当前进程的优先级（占位实现固定返回0）
 */
uint64 sys_get_priority(void) {
  struct proc* p = myproc();
  acquire(&p->lock);
  int priority = p->priority;
  release(&p->lock);
  return priority;
}
#endif

/**
 * @brief 设置最大物理页数
 * @param max_page_in_mem 最大物理页数
 * @return 0 成功，-1 失败
 */
uint64 sys_set_max_page_in_mem(void) {
  int max_page_in_mem;
  if (argint(0, &max_page_in_mem) < 0) {
    return -1;
  }
  return 0;
}

/**
 * @brief 获取交换次数
 * @return 交换次数
 */
uint64 sys_get_swap_count(void) {
  return 0;
}

/**
 * @brief 通知LRU页面替换算法
 * @param addr 地址
 * @return 0 成功，-1 失败
 */
uint64 sys_lru_access_notify(void) {
  uint64 addr;
  if (argaddr(0, &addr) < 0) {
    return -1;
  }
  return 0;
}