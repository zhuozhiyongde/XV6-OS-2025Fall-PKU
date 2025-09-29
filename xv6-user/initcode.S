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
