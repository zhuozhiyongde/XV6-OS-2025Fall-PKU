
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

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
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

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
 * @brief 从系统调用参数中获取一个用户空间的目标地址，然后将内核中的某块数据拷贝到这个目标地址去。
 * @param arg_index 系统调用参数的索引
 * @param dest 目标地址
 * @param size 数据大小
 * @return 0 成功，-1 失败
 */
int get_and_copyout(uint64 arg_index, void* dest, int size) {
  uint64 addr;
  if (argaddr(arg_index, &addr) < 0) {
    return -1;
  }
  if (copyout2(addr, dest, size) < 0) {
    return -1;
  }
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

  if (get_and_copyout(0, (void*)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 实现 times 系统调用，返回自启动以来经过的 tick 数。
 * @param addr 目标地址
 * @return 0 成功，-1 失败
 */
uint64 sys_times(void) {
  struct tms tms;

  acquire(&tickslock);
  tms.tms_utime = tms.tms_stime = tms.tms_cutime = tms.tms_cstime = ticks;
  release(&tickslock);

  if (get_and_copyout(0, (void*)&tms, sizeof(tms)) < 0) {
    return -1;
  }

  return 0;
}
