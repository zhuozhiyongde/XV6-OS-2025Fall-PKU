#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sbi.h"
#include "include/plic.h"
#include "include/trap.h"
#include "include/syscall.h"
#include "include/printf.h"
#include "include/console.h"
#include "include/timer.h"
#include "include/disk.h"
#include "include/vm.h"
#include "include/kalloc.h"
#include "include/string.h"

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
extern void kernelvec();

int devintr();

static int handle_user_page_fault(struct proc *p, uint64 scause, uint64 stval);
static int vma_handler(struct proc *p, uint64 scause, uint64 stval);
static int lazy_handler(struct proc *p, uint64 stval);
static int cow_handler(struct proc *p, uint64 scause, uint64 stval);

// void
// trapinit(void)
// {
//   initlock(&tickslock, "time");
//   #ifdef DEBUG
//   printf("trapinit\n");
//   #endif
// }

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  // enable supervisor-mode timer interrupts.
  w_sie(r_sie() | SIE_SEIE | SIE_SSIE | SIE_STIE);
  set_next_timeout();
  #ifdef DEBUG
  printf("trapinithart\n");
  #endif
}

/**
 * @brief 处理写时复制时的缺页异常
 * @param p 进程
 * @param scause 异常原因
 * @param stval 异常地址
 * @return 0 成功，-1 失败
 */
static int
cow_handler(struct proc *p, uint64 scause, uint64 stval)
{
  // scause 15：存储 / AMO 页面故障
  // 写时复制异常只能是存储访问异常
  if (scause != 15) {
    return -1;
  }

  uint64 va = PGROUNDDOWN(stval);
  pte_t *pte = walk(p->pagetable, va, 0);
  if (pte == 0) {
    return -1;
  }
  // 页表项不是 COW 页，直接返回
  if ((*pte & PTE_COW) == 0) {
    return -1;
  }

  // 处理写时复制
  if (cow_make_writable(p, va) < 0) {
    printf("cow_handler(): out of memory\n");
    p->killed = 1;
  }
  return 0;
}

/**
 * @brief 处理虚拟内存区域异常，包括 mmap 区缺页或者保护错误
 * @param p 进程
 * @param scause 异常原因
 * @param stval 异常地址
 * @return 0 成功，-1 失败
 */
static int
vma_handler(struct proc *p, uint64 scause, uint64 stval)
{
  struct vma* v = 0;
  for (int i = 0; i < NVMA; i++) {
    if (p->vmas[i].valid && stval >= p->vmas[i].start && stval < p->vmas[i].end) {
      v = &p->vmas[i];
      break;
    }
  }

  // 没有找到对应的 VMA，返回错误
  if (v == 0) {
    return -1;
  }

  // 找到了对应的 VMA，检查权限
  if (
    (scause == 12 && !(v->prot & PROT_EXEC)) || // 取指缺页，但 VMA 不可执行
    (scause == 13 && !(v->prot & PROT_READ)) || // 读缺页，但 VMA 不可读
    (scause == 15 && !(v->prot & PROT_WRITE)) // 写缺页，但 VMA 不可写
  ) {
    printf("vma_handler(): protection fault pid=%d %s, va=%p\n", p->pid, p->name, stval);
    p->killed = 1;
    return 0;
  }

  // 以下处理由于 VMA 懒分配导致的缺页异常，按需分配物理页并映射到用户页表、内核页表
  // 计算缺页地址所在的页的起始地址
  uint64 va_page_start = PGROUNDDOWN(stval);
  // 分配一页物理内存
  char* mem = kalloc();
  // 分配失败，返回错误
  if (mem == 0) {
    printf("vma_handler(): out of memory\n");
    p->killed = 1;
    return 0;
  }
  // 将新分配的页清零
  memset(mem, 0, PGSIZE);

  // 如果是文件映射，从文件中读取相应内容到新分配的页
  if (v->vm_file) {
    elock(v->vm_file->ep);
    // 计算文件内的偏移量：VMA 文件偏移 + 页在 VMA 内的偏移
    uint64 file_offset = v->offset + (va_page_start - v->start);
    // 从文件读取一页内容到内核地址 mem
    eread(v->vm_file->ep, 0, (uint64)mem, file_offset, PGSIZE);
    eunlock(v->vm_file->ep);
  }

  // 根据 VMA 的保护权限，设置页表项 PTE 的标志位
  int pte_flags = PTE_U; // PTE_U 表示用户态可访问
  if (v->prot & PROT_READ) pte_flags |= PTE_R;
  if (v->prot & PROT_WRITE) pte_flags |= PTE_W;
  if (v->prot & PROT_EXEC) pte_flags |= PTE_X;

  // 调用 mappages 将物理页 mem 映射到用户虚拟地址 va_page_start
  if (mappages(p->pagetable, va_page_start, PGSIZE, (uint64)mem, pte_flags) != 0) {
    // 映射失败，释放刚分配的页
    kfree(mem);
    printf("vma_handler(): mappages failed\n");
    p->killed = 1;
    return 0;
  }
  // 同样需要映射到内核页表
  if (mappages(p->kpagetable, va_page_start, PGSIZE, (uint64)mem, pte_flags & ~PTE_U) != 0) {
    kfree(mem);
    vmunmap(p->pagetable, va_page_start, 1, 1);
    printf("vma_handler(): kernel mappages failed\n");
    p->killed = 1;
    return 0;
  }

  return 0;
}

/**
 * @brief 处理堆懒分配的缺页异常
 * @param p 进程
 * @param stval 异常地址
 * @return 0 成功，-1 失败
 */
static int
lazy_handler(struct proc *p, uint64 stval)
{
  // 地址超出堆上限地址或者 mmap 区上限地址，返回错误
  if (stval >= p->sz || stval >= MMAPBASE) {
    return -1;
  }

  // 仿照 vma_handler 的逻辑，分配一页物理内存，并映射到用户页表、内核页表
  uint64 va_page_start = PGROUNDDOWN(stval);
  pte_t* pte = walk(p->pagetable, va_page_start, 0);
  // 如果查到有效 PTE，说明实际已经分配了物理页，直接返回错误
  if (pte && (*pte & PTE_V)) {
    return -1;
  }

  // 分配一页物理内存
  char* mem = kalloc();
  // 分配失败，返回错误
  if (mem == 0) {
    printf("lazy_handler(): out of memory\n");
    p->killed = 1;
    return 0;
  }
  // 将新分配的页清零
  memset(mem, 0, PGSIZE);

  // 用户态页表项标志位
  int pte_flags = PTE_W | PTE_X | PTE_R | PTE_U;
  if (mappages(p->pagetable, va_page_start, PGSIZE, (uint64)mem, pte_flags) != 0) {
    kfree(mem);
    printf("lazy_handler(): mappages failed\n");
    p->killed = 1;
    return 0;
  }
  // 内核态页表项标志位
  int kpte_flags = pte_flags & ~PTE_U;
  if (mappages(p->kpagetable, va_page_start, PGSIZE, (uint64)mem, kpte_flags) != 0) {
    kfree(mem);
    vmunmap(p->pagetable, va_page_start, 1, 1);
    printf("lazy_handler(): kernel mappages failed\n");
    p->killed = 1;
    return 0;
  }

  return 0;
}

/**
 * @brief 处理用户页缺页异常
 * @param p 进程
 * @param scause 异常原因
 * @param stval 异常地址
 * @return 0 成功，-1 失败
 */
static int
handle_user_page_fault(struct proc *p, uint64 scause, uint64 stval)
{
  if (cow_handler(p, scause, stval) == 0) {
    return 0;
  }
  if (vma_handler(p, scause, stval) == 0) {
    return 0;
  }
  if (lazy_handler(p, stval) == 0) {
    return 0;
  }
  return -1;
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  // printf("run in usertrap\n");
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();

  // 系统调用，r_scause() == 8，即 syscall
  if (r_scause() == 8) {
    // system call
    if(p->killed)
      exit(-1);
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    syscall();
  } 
  // 外部中断或设备中断，r_scause() != 8
  else if((which_dev = devintr()) != 0){
    // ok
  } 
  else {
    // 获取触发异常的原因和地址
    uint64 scause = r_scause();
    uint64 stval = r_stval(); // stval 寄存器保存了导致异常的地址

    // 检查是否为缺页异常
    // 13: Load page fault (读缺页)
    // 15: Store/AMO page fault (写缺页)
    // 12: Instruction page fault (取指缺页)
    if (scause == 12 || scause == 13 || scause == 15) {
      if (handle_user_page_fault(p, scause, stval) < 0) {
        printf("usertrap(): segfault pid=%d %s, va=%p\n", p->pid, p->name, stval);
        p->killed = 1;
      }
    }
    else {
      // 如果不是缺页异常，按原逻辑处理未知异常
      printf("\nusertrap(): unexpected scause %p pid=%d %s\n", r_scause(), p->pid, p->name);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      p->killed = 1;
    }
  }

  if(p->killed)
    exit(-1);

  // 时钟中断，需要进行调度
  if (which_dev == 2) {
    #ifdef SCHEDULER_RR
    // RR 算法：进入时间中断后，处理时间片递减与抢占逻辑
    rr_on_timer_tick();
    #elif defined(SCHEDULER_MLFQ)
    // MLFQ 算法：进入时间中断后，处理时间片递减、动态优先级调整与抢占逻辑
    mlfq_on_timer_tick();
    #else
    // 默认：直接让出 CPU
    yield();
    #endif
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // printf("[usertrapret]p->pagetable: %p\n", p->pagetable);
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap() {
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("\nscause %p\n", scause);
    printf("sepc=%p stval=%p hart=%d\n", r_sepc(), r_stval(), r_tp());
    struct proc *p = myproc();
    if (p != 0) {
      printf("pid: %d, name: %s\n", p->pid, p->name);
    }
    panic("kerneltrap");
  }
  // printf("which_dev: %d\n", which_dev);
  
  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2) {
    #ifdef SCHEDULER_RR
    // RR 算法：进入时间中断后，处理时间片递减与抢占逻辑
    rr_on_timer_tick();
    #elif defined(SCHEDULER_MLFQ) 
    // MLFQ 算法：进入时间中断后，处理时间片递减、动态优先级调整与抢占逻辑
    mlfq_on_timer_tick();
    #else
    // 默认：直接让出 CPU
    if (myproc() != 0 && myproc()->state == RUNNING) {
      yield();
    }
    #endif
  }
  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// Check if it's an external/software interrupt, 
// and handle it. 
// returns  2 if timer interrupt, 
//          1 if other device, 
//          0 if not recognized. 
int devintr(void) {
	uint64 scause = r_scause();

	#ifdef QEMU 
	// handle external interrupt 
	if ((0x8000000000000000L & scause) && 9 == (scause & 0xff)) 
	#else 
	// on k210, supervisor software interrupt is used 
	// in alternative to supervisor external interrupt, 
	// which is not available on k210. 
	if (0x8000000000000001L == scause && 9 == r_stval()) 
	#endif 
	{
		int irq = plic_claim();
		if (UART_IRQ == irq) {
			// keyboard input 
			int c = sbi_console_getchar();
			if (-1 != c) {
				consoleintr(c);
			}
		}
		else if (DISK_IRQ == irq) {
			disk_intr();
		}
		else if (irq) {
			printf("unexpected interrupt irq = %d\n", irq);
		}

		if (irq) { plic_complete(irq);}

		#ifndef QEMU 
		w_sip(r_sip() & ~2);    // clear pending bit
		sbi_set_mie();
		#endif 

		return 1;
	}
	else if (0x8000000000000005L == scause) {
		timer_tick();
		return 2;
	}
	else { return 0;}
}

void trapframedump(struct trapframe *tf)
{
  printf("a0: %p\t", tf->a0);
  printf("a1: %p\t", tf->a1);
  printf("a2: %p\t", tf->a2);
  printf("a3: %p\n", tf->a3);
  printf("a4: %p\t", tf->a4);
  printf("a5: %p\t", tf->a5);
  printf("a6: %p\t", tf->a6);
  printf("a7: %p\n", tf->a7);
  printf("t0: %p\t", tf->t0);
  printf("t1: %p\t", tf->t1);
  printf("t2: %p\t", tf->t2);
  printf("t3: %p\n", tf->t3);
  printf("t4: %p\t", tf->t4);
  printf("t5: %p\t", tf->t5);
  printf("t6: %p\t", tf->t6);
  printf("s0: %p\n", tf->s0);
  printf("s1: %p\t", tf->s1);
  printf("s2: %p\t", tf->s2);
  printf("s3: %p\t", tf->s3);
  printf("s4: %p\n", tf->s4);
  printf("s5: %p\t", tf->s5);
  printf("s6: %p\t", tf->s6);
  printf("s7: %p\t", tf->s7);
  printf("s8: %p\n", tf->s8);
  printf("s9: %p\t", tf->s9);
  printf("s10: %p\t", tf->s10);
  printf("s11: %p\t", tf->s11);
  printf("ra: %p\n", tf->ra);
  printf("sp: %p\t", tf->sp);
  printf("gp: %p\t", tf->gp);
  printf("tp: %p\t", tf->tp);
  printf("epc: %p\n", tf->epc);
}
