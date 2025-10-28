# XV6-OS-2025Fall-PKU

北京大学 2025 秋季学期操作系统课程 Lab 代码、笔记、经验。

```
  (`-.            (`-.                            .-')       ('-.    _   .-')
 ( OO ).        _(OO  )_                        .(  OO)    _(  OO)  ( '.( OO )_ 
(_/.  \_)-. ,--(_/   ,. \  ,--.                (_)---\_)  (,------.  ,--.   ,--.) ,--. ,--.  
 \  `.'  /  \   \   /(__/ /  .'       .-')     '  .-.  '   |  .---'  |   `.'   |  |  | |  |   
  \     /\   \   \ /   / .  / -.    _(  OO)   ,|  | |  |   |  |      |         |  |  | | .-')
   \   \ |    \   '   /, | .-.  '  (,------. (_|  | |  |  (|  '--.   |  |'.'|  |  |  |_|( OO )
  .'    \_)    \     /__)' \  |  |  '------'   |  | |  |   |  .--'   |  |   |  |  |  | | `-' /
 /  .'.  \      \   /    \  `'  /              '  '-'  '-. |  `---.  |  |   |  | ('  '-'(_.-'
'--'   '--'      `-'      `----'                `-----'--' `------'  `--'   `--'   `-----'
```

**建议忽略官方文档，直接从本仓库开始试验。**

本仓库完整的融合了官方文档和 [测试仓库文档](./oscomp_syscalls.md) 中的有效信息，对于几乎所有实现的系统调用讲明白了原理和实现细节，并给出了相应的代码注释，希望能够让后来者不必再迷茫困惑于信息、知识的缺乏。

在本仓库的基础上，基本上只要你学过 ICS 课程，就可以无障碍零基础的开始完成这个 Lab。

## ✨ 快速开始

```shell
git clone https://github.com/zhuozhiyongde/XV6-OS-2025Fall-PKU.git
cd XV6-OS-2025Fall-PKU
git branch -m ref # 将 main 分支重命名为 ref
git branch main $(git rev-list --max-parents=0 HEAD) # 基于最开始的 commit 创建一个新的 main 分支
```

### 🛠️ Makefile

建议先修改 `initcode.S` 为如下 64 位 RISC-V 的写法，使之可以正确拉起 `/init` 程序：

```assembly
# Initial process that execs /init.
# This code runs in user space.

#include "include/sysnum.h"

  .text
  .option nopic

# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall

# for(;;) exit();
exit:
        li a7, SYS_exit
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .asciz "/init"

# char *argv[] = { init, 0 };
.section .rodata
.p2align 2
argv:
  .dword init
  .dword 0

```

然后，在 `Makefile` 中添加如下配置，从而进入容器后可以直接使用 `make local` 命令来启动完整的本地测试（此时是经由自举代码拉起 `/init` 进程），并且同时支持在希冀平台上进行正确评测（此时是硬编码完整的 `init.c` 程序的机器码到 `initcode.h` 中），且都允许 `init.c` 中的修改立刻生效。

> 这个差异的因为希冀平台评测时所提供的预编译 `sdcard.img` 中没有 `init` 程序，我们无法通过自举代码来拉起 `/init` 进程，必须硬编码。


```makefile
# 如果是提交到希冀平台，因为平台提供的 sdcard.img 挂载里没有 init.c 文件
# 所以需要硬编码完整的 init.c 程序的机器码到 initcode.h 中
HARD_CODE_INIT = 0

ifeq ($(HARD_CODE_INIT), 1)
dump: userprogs
  @echo "HARD_CODE_INIT is 1, compile the entire init.c program into initcode.h directly."
  @$(TOOLPREFIX)objcopy -S -O binary $U/_init tmp_initcode
  @od -v -t x1 -An tmp_initcode | sed -E 's/ (.{2})/0x\1,/g' > kernel/include/initcode.h 
  @rm tmp_initcode
else
dump: $U/initcode
  @echo "HARD_CODE_INIT is 0, compile the bootstrap fragment initcode.S normally."
  @od -v -t x1 -An $U/initcode | sed -E 's/ (.{2})/0x\1,/g' > kernel/include/initcode.h
endif

# 希冀平台所使用的编译命令
all:
  @$(MAKE) clean
  @$(MAKE) dump HARD_CODE_INIT=1
  @$(MAKE) build
  @cp $(T)/kernel ./kernel-qemu
  @cp ./bootloader/SBI/sbi-qemu ./sbi-qemu

# 助教提供的功能性测试平台所使用的编译命令
run_test:
  @$(MAKE) clean
  @$(MAKE) dump ENABLE_JUDGER=1
  @$(MAKE) build ENABLE_JUDGER=1
  @$(MAKE) fs ENABLE_JUDGER=1
  @$(MAKE) run ENABLE_JUDGER=1

# 本地测试所使用的编译命令
local:
  @$(MAKE) clean
  @$(MAKE) dump
  @$(MAKE) build
  @$(MAKE) fs
  @$(MAKE) run
```

出于简便起见，你可以直接使用别名（Alias），通过在 `~/.bashrc` 或者 `~/.zshrc` 中添加如下内容：

```shell
alias qwe='docker run -ti --rm -v ./:/xv6 -w /xv6 --privileged=true docker.educg.net/cg/os-contest:2024p6 /bin/bash'
```

然后重载一下 Shell（删了重新创建，或者使用 `. ~/.bashrc` 或者 `. ~/.zshrc`）。

这样，你每次就可以直接使用 `qwe` 进入 Docker 环境了。

（详细操作见 [part0.md](./notes/part0.md)）

## 📝 各部分简要说明

### Part 0

主要完成了 Docker 环境搭建、Make 工具配置、本地测试环境搭建、远程测试环境搭建，以及项目基础运行逻辑的梳理介绍。

详细内容参见 [xv6-os-lab-part0 笔记](https://arthals.ink/blog/xv6-os-lab-part0)。

### Part 1

完成 `initcode.h` 的自动化生成，实现如下系统调用：

-   `getcwd`
-   `write`
-   `getpid`
-   `times`
-   `uname`

详细内容参见 [xv6-os-lab-part1 笔记](https://arthals.ink/blog/xv6-os-lab-part1)。

### Part 2

详细介绍了内核中基础文件系统与内存机制，并实现了如下系统调用：

-   `open`
-   `openat`
-   `brk`
-   `mmap`
-   `munmap`

详细内容参见 [xv6-os-lab-part2 笔记](https://arthals.ink/blog/xv6-os-lab-part2)。

### Part 3

详细介绍了时钟与进程管理相关机制，并实现了如下系统调用：

-   `wait`
-   `waitpid`
-   `clone`
-   `fork`
-   `execve`
-   `getppid`
-   `exit`
-   `yield`
-   `gettimeofday`
-   `sleep`

详细内容参见 [xv6-os-lab-part3 笔记](https://arthals.ink/blog/xv6-os-lab-part3)。


### Part 7

详细介绍了 FAT32 文件系统相关机制，并实现了如下系统调用：

- `dup`
- `dup3`
- `pipe`
- `close`
- `getdents`
- `read`
- `mkdirat`
- `chdir`
- `unlinkat`
- `mount`
- `umount2`
- `fstat`

详细内容参见 [xv6-os-lab-part7 笔记](https://arthals.ink/blog/xv6-os-lab-part7)。

> 你也可以在 `notes/` 目录下查看完整的笔记源代码，但推荐在我的博客中查看以获得更好的阅读体验。

## 📜 LICENSE

MIT License