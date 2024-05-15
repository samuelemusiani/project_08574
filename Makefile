# Cross toolchain variables
# If these are not in your path, you can make them absolute.
XT_PRG_PREFIX = /opt/riscv/bin/riscv32-unknown-linux-gnu-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld

URISCV_DIR_PREFIX = /usr/local

URISCV_DATA_DIR = $(URISCV_DIR_PREFIX)/share/uriscv
URISCV_INCLUDE_DIR = $(URISCV_DIR_PREFIX)/include

# Compiler options
CFLAGS_LANG = -ffreestanding -static -nostartfiles -nostdlib -Werror -ansi -g3 -gdwarf-2
CFLAGS = $(CFLAGS_LANG) -I$(URISCV_INCLUDE_DIR) -ggdb -Wall -O0 -std=gnu99 -march=rv32imfd -mabi=ilp32d

# Linker options
LDFLAGS = -G 0 -nostdlib -T $(URISCV_DATA_DIR)/uriscvcore.ldscript

# Add the location of crt*.S to the search path
VPATH = $(URISCV_DATA_DIR)

.PHONY : all clean

all : kernel.core.uriscv testers

testers:
	$(MAKE) -C phase3/testers

kernel.core.uriscv : kernel
	uriscv-elf2uriscv -k $<

kernel : ./phase1/msg.o ./phase1/pcb.o crtso.o liburiscv.o \
	./phase2/exceptions.o ./phase2/initial.o ./phase2/interrupts.o \
	./phase2/scheduler.o ./phase2/ssi.o ./phase2/utils.o \
	./phase3/initProc.o ./phase3/sysSupport.o ./phase3/sst.o \
	./phase3/vmSupport.o ./phase3/utils3.o ./phase3/support.o
	$(LD) -o $@ $^ $(LDFLAGS)

clean :
	-rm -f *.o ./phase1/*.o ./phase2/*.o ./phase3/*.o kernel *.uriscv
	$(MAKE) -C phase3/testers clean

# Pattern rule for assembly modules
%.o : %.S
	$(CC) $(CFLAGS) -c -o $@ $<
