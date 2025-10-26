#ifndef __PROC_H
#define __PROC_H

#include "param.h"
#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "file.h"
#include "fat32.h"
#include "trap.h"
#include "vm.h"

#ifdef SCHEDULER_RR
#define DEFAULT_TIMESLICE 1
#endif
#if defined(SCHEDULER_PRIORITY) || defined(SCHEDULER_MLFQ)
#define DEFAULT_PRIORITY 5 // 默认分配给新进程的优先级
#endif
#ifdef SCHEDULER_MLFQ 
#define MLFQ_MIN_PRIORITY_LEVEL 1 // MLFQ：最高优先级对应的数值
#define MLFQ_MAX_PRIORITY_LEVEL 20 // MLFQ：最低优先级对应的数值
#define MLFQ_EVAL_TICKS 5 // MLFQ：统计窗口长度，单位为 tick
#define MLFQ_CPU_DOM_RATIO 2 // MLFQ：CPU 压制阈值，CPU 使用超过休眠两倍视为 CPU 密集
#define MLFQ_SLEEP_DOM_RATIO 2 // MLFQ：休眠压制阈值，休眠超过 CPU 两倍视为 I/O 密集

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  pagetable_t kpagetable;      // Kernel page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct dirent *cwd;          // Current directory
  char name[16];               // Process name (debugging)
  int tmask;                    // trace mask
  
  #ifdef SCHEDULER_RR
  // RR 算法相关 PCB 数据结构扩展
  int timeslice;                // 进程设定的基础时间片长度
  int slice_remaining;          // 当前调度周期内剩余的时间片
  #endif

  #ifdef SCHEDULER_PRIORITY
  // 优先级调度所需的 PCB 字段
  int priority;                 // 数值越小代表优先级越高
  #endif

  #ifdef SCHEDULER_MLFQ
  // MLFQ 算法所需的 PCB 字段
  int priority;                 // 动态优先级，数值越小代表优先级越高
  int ticks_used;               // 记录当前时间片已消耗的 tick 数
  int eval_ticks;               // 当前统计窗口内累计 tick 数
  int cpu_ticks;                // 最近窗口内的 CPU 使用 tick 数
  int sleep_ticks;              // 最近窗口内的休眠 tick 数
  int base_priority;            // 记录用户设置的基础优先级，用于同级队列的 FIFO 判定
  #endif

  // vma 相关
  struct vma vmas[NVMA];
};

void            reg_info(void);
int             cpuid(void);
void            exit(int);
int             fork(void);
int             clone(void);
int             growproc(int);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
int             kill(int);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(int, uint64);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
uint64          procnum(void);
void            test_proc_init(int);

#ifdef SCHEDULER_RR
void            rr_on_timer_tick(void);
#endif

#ifdef SCHEDULER_MLFQ
int             mlfq_clamp_priority(int priority);
void            mlfq_on_timer_tick(void);
void            mlfq_account_sleep(struct proc *p, int sleep_ticks);
#endif

#endif
