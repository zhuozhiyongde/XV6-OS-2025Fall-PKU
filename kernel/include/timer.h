#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"
#include "spinlock.h"

extern struct spinlock tickslock;
// ticks 是全局维护的时间 tick 数，维护方式参见 kernel/timer.c/timer_tick() 及我所写的 Note
// 简而言之就是每次时钟中断时触发，上锁修改，然后设置下一次中断时间，如此递归
extern uint ticks;

// 用于 sys_times 系统调用所定义的结构体
// ref: https://man7.org/linux/man-pages/man2/times.2.html
struct tms {
    long tms_utime; // 用户态时间，user time
    long tms_stime; // 系统态时间，system time
    long tms_cutime; // 子进程用户态时间，child user time
    long tms_cstime; // 子进程系统态时间，child system time
};

void timerinit();
void set_next_timeout();
void timer_tick();

#endif
