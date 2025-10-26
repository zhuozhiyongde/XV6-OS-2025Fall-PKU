# platform	:= k210
platform	:= qemu
# mode := debug
mode := release
K=kernel
U=xv6-user
T=target
TEST=xv6-user/testcases

OBJS = $K/entry_qemu.o

OBJS += \
  $K/printf.o \
  $K/kalloc.o \
  $K/intr.o \
  $K/spinlock.o \
  $K/string.o \
  $K/main.o \
  $K/vm.o \
  $K/proc.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/bio.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/timer.o \
  $K/disk.o \
  $K/fat32.o \
  $K/plic.o \
  $K/console.o

OBJS += \
  $K/virtio_disk.o \
  #$K/uart.o \

QEMU = qemu-system-riscv64

RUSTSBI = ./bootloader/SBI/sbi-qemu

# TOOLPREFIX	:= riscv64-unknown-elf-
TOOLPREFIX	:= riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -g
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

ifeq ($(mode), debug) 
CFLAGS += -DDEBUG 
endif 

CFLAGS += -D QEMU

# Part 4: 选择调度器类型
USER_CFLAGS = 
SCHEDULER_TYPE ?= PRIORITY
TEST_PROGRAM = 

ifeq ($(SCHEDULER_TYPE), RR)
  TEST_PROGRAM = test_proc_rr
  CFLAGS += -DSCHEDULER_RR
  USER_CFLAGS += -DSCHEDULER_RR
else ifeq ($(SCHEDULER_TYPE), PRIORITY)
  TEST_PROGRAM = test_proc_priority
  CFLAGS += -DSCHEDULER_PRIORITY
  USER_CFLAGS += -DSCHEDULER_PRIORITY
else ifeq ($(SCHEDULER_TYPE), MLFQ)
  TEST_PROGRAM = test_proc_mlfq
  CFLAGS += -DSCHEDULER_MLFQ
  USER_CFLAGS += -DSCHEDULER_MLFQ
endif

TEST_PROGRAM := $(strip $(TEST_PROGRAM))
CFLAGS += -DTEST_PROGRAM=\"$(TEST_PROGRAM)\"
USER_CFLAGS += -DTEST_PROGRAM=\"$(TEST_PROGRAM)\"

# END Part 4

LDFLAGS = -z max-page-size=4096

linker = ./linker/qemu.ld

# Compile Kernel
$T/kernel: $(OBJS) $(linker) $U/initcode
	@if [ ! -d "./target" ]; then mkdir target; fi
	@$(LD) $(LDFLAGS) -T $(linker) -o $T/kernel $(OBJS)
	@$(OBJDUMP) -S $T/kernel > $T/kernel.asm
	@$(OBJDUMP) -t $T/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $T/kernel.sym
  
build: $T/kernel userprogs

# Compile RustSBI
RUSTSBI:
	@cd ./bootloader/SBI/rustsbi-qemu && cargo build && cp ./target/riscv64gc-unknown-none-elf/debug/rustsbi-qemu ../sbi-qemu
	@$(OBJDUMP) -S ./bootloader/SBI/sbi-qemu > $T/rustsbi-qemu.asm

rustsbi-clean:
	@cd ./bootloader/SBI/rustsbi-k210 && cargo clean
	@cd ./bootloader/SBI/rustsbi-qemu && cargo clean

image = $T/kernel.bin

ifndef CPUS
CPUS := 1
endif

QEMUOPTS = -machine virt -kernel $T/kernel -m 32M -nographic

# use multi-core 
QEMUOPTS += -smp $(CPUS)

QEMUOPTS += -bios $(RUSTSBI)

# import virtual disk image
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0 
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

run: build
	@$(QEMU) $(QEMUOPTS)

$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

tags: $(OBJS) _init
	@etags *.S *.c

ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/usys.pl
	@perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

$U/_forktest: $U/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
	$U/_init\
	$U/_sh\
	$U/_cat\
	$U/_echo\
	$U/_grep\
	$U/_ls\
	$U/_kill\
	$U/_mkdir\
	$U/_xargs\
	$U/_sleep\
	$U/_find\
	$U/_rm\
	$U/_wc\
	$U/_test\
	$U/_usertests\
	$U/_strace\
	$U/_mv\

	# $U/_forktest\
	# $U/_ln\
	# $U/_stressfs\
	# $U/_grind\
	# $U/_zombie\

# Part4 调度器测试程序
TESTCASES := $(TEST)/_judger
ifneq ($(TEST_PROGRAM),)
  TESTCASES += $(TEST)/_$(TEST_PROGRAM)
endif

$(TEST)/%: $(TEST)/%.c $(ULIB)
	$(CC) $(USER_CFLAGS) -o $@ $<
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^

userprogs: $(UPROGS)

dst=/mnt

# @cp $U/_init $(dst)/init
# @cp $U/_sh $(dst)/sh
# Make fs image
fs: $(UPROGS) $(TESTCASES)
	@if [ ! -f "fs.img" ]; then \
		echo "making fs image..."; \
		dd if=/dev/zero of=fs.img bs=512k count=512; \
		mkfs.vfat -F 32 fs.img; fi
	@mount fs.img $(dst)
	@if [ ! -d "$(dst)/bin" ]; then mkdir $(dst)/bin; fi
	@cp README $(dst)/README
	@for file in $$( ls $U/_* ); do \
		cp $$file $(dst)/$${file#$U/_};\
		cp $$file $(dst)/bin/$${file#$U/_}; done
	@for file in $$( ls $(TEST)/_* ); do \
		cp $$file $(dst)/$${file#$(TEST)/_}; \
	done
	@cp -r riscv64/* $(dst)
	@umount $(dst)

# Write mounted sdcard
sdcard: userprogs
	@if [ ! -d "$(dst)/bin" ]; then mkdir $(dst)/bin; fi
	@for file in $$( ls $U/_* ); do \
		cp $$file $(dst)/bin/$${file#$U/_}; done
	@cp $U/_init $(dst)/init
	@cp $U/_sh $(dst)/sh
	@cp README $(dst)/README

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

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$T/* \
	$U/initcode $U/initcode.out \
	$K/kernel \
	.gdbinit \
	$U/usys.S \
	$(UPROGS)
	rm -f $(TEST)/*.d $(TEST)/*.o $(TEST)/*.asm $(TEST)/*.sym $(TEST)/_*
	
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
	@$(MAKE) dump
	@$(MAKE) build
	@$(MAKE) fs
	@$(MAKE) run

# 本地测试所使用的编译命令
local:
	@$(MAKE) clean
	@$(MAKE) dump
	@$(MAKE) build
	@$(MAKE) fs
	@$(MAKE) run

QEMUOPTS_GDB = $(QEMUOPTS) -S -s

gdb: build .gdbinit
	@echo "====== Running QEMU with GDB support... ======"
	@echo "In another terminal, run: 'gdb-multiarch $(T)/kernel', then type 'c' to continue"
	@echo "=============================================="
	@$(QEMU) $(QEMUOPTS_GDB)

.gdbinit:
	@echo "target remote localhost:1234" > .gdbinit
	@echo "set architecture riscv:rv64" >> .gdbinit
	@echo "symbol-file $(T)/kernel" >> .gdbinit
	@echo "set auto-load safe-path /xv6" > ~/.gdbinit
