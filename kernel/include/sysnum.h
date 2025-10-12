#ifndef __SYSNUM_H
#define __SYSNUM_H

// System call numbers


// Filesystem related (文件系统相关)
#define SYS_open        55   // 打开文件，基于当前目录或者直接使用绝对路径
#define SYS_openat      56   // 打开文件，基于指定的 fd 所代表的目录或者直接使用绝对路径
#define SYS_close       57   // 关闭文件
#define SYS_read        63   // 从文件读取数据
#define SYS_write       64   // 向文件写入数据
#define SYS_fstat       80   // 获取文件状态
#define SYS_pipe        59   // 创建管道
#define SYS_dup         23   // 复制文件描述符
#define SYS_dup3        24   // 复制文件描述符，支持重定向
#define SYS_mkdir        7   // 创建目录
#define SYS_mkdirat     34   // 在指定目录下创建目录
#define SYS_chdir       49   // 改变当前工作目录
#define SYS_getcwd      17   // 获取当前工作目录
#define SYS_readdir     27   // 读取目录项
#define SYS_rename      26   // 重命名文件或目录
#define SYS_remove      117  // 删除文件或目录
#define SYS_unlinkat    35   // 在指定目录下删除文件
#define SYS_dev         21   // 设备文件操作
#define SYS_getdents    61   // 读取目录项
#define SYS_mount       40   // 挂载文件系统
#define SYS_umount      39   // 卸载文件系统


// Process management related (进程管理相关)
#define SYS_fork         1   // 创建子进程
#define SYS_clone      220   // 创建子进程/线程（更灵活的fork）
#define SYS_exec       221   // 执行新程序
#define SYS_exit        93   // 终止当前进程
#define SYS_wait         3   // 等待子进程结束
#define SYS_waitpid    260   // 等待子进程结束（更通用的版本）
#define SYS_kill         6   // 向进程发送信号
#define SYS_getpid     172   // 获取当前进程ID
#define SYS_getppid    173   // 获取父进程ID
#define SYS_sleep       13   // 使进程休眠（秒）
#define SYS_nanosleep  101   // 使进程休眠（纳秒）
#define SYS_sched_yield 124  // 主动让出CPU
#define SYS_times      153   // 获取进程的执行时间


// Memory management related (内存管理相关)
#define SYS_sbrk        12   // 调整程序数据段（堆）的大小
#define SYS_brk        214   // 直接设置程序数据段的结束地址
#define SYS_munmap     215   // 释放内存映射
#define SYS_mmap       222   // 映射文件或设备到内存


// Others (其他)
#define SYS_gettimeofday 169 // 获取当前时间
#define SYS_uptime      14   // 获取系统自启动以来的运行时间
#define SYS_sysinfo     19   // 获取通用系统信息
#define SYS_uname      160   // 获取操作系统名称和版本等信息
#define SYS_shutdown   210   // 关闭系统
#define SYS_trace       18   // 用于调试，追踪系统调用
#define SYS_test_proc   22   // 自定义的测试调用

#endif