#ifndef __PARAM_H
#define __PARAM_H

#define NPROC        50  // maximum number of processes
#define NCPU          2  // maximum number of CPUs
#define NOFILE      256  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
#define MAXPATH      260   // maximum file path name
/* 
注意区分硬件 tick 和操作系统 tick：
- 硬件 tick：通过 r_time() 获取到的 tick 数，按照 CLOCK_FREQ 频率增长
- 操作系统 tick：通过 ticks 全局变量获取到的 tick 数，每 INTERVAL 个硬件 tick 增长 1 次

通过 README 或者在容器内获取设备树可以发现 QEMU 的硬件时钟计数器的更新频率（timebase-frequency，即 r_time() 获取到的硬件 tick 更新频率）为 virt 机器默认的 10000000 Hz（10 MHz）
而读取 bootloader/SBI/rustsbi-k210/kendryte-k210.dtsi 可以发现 K210 的硬件时钟计数器的更新频率为 7800000 Hz（7.8 MHz），所以我们发现这里实际上是原来是每 50 秒 200 个时钟中断，也即每秒 4 个时钟中断，ticks 这个全局变量按照 4ticks/s 增长，非常慢
所以这里改写为 (CLOCK_FREQ / 200)，即每秒触发 200 个时钟中断，也即 200 ticks/s，ticks 这个全局变量按照 200ticks/s 增长
*/
// #define INTERVAL     (390000000 / 200) // timer interrupt interval
#define CLOCK_FREQ   10000000 // 10 MHz
#define TICKS_PER_SECOND    200 // 每秒时钟中断次数
#define INTERVAL     (CLOCK_FREQ / TICKS_PER_SECOND) // timer interrupt interval

#endif