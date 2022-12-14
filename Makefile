.PHONY: clean build user run debug test .FORCE
all: build

ifndef CPUS
CPUS := 4
endif

DEBUG_MODE ?= 0

K = os
#U = user
F = nfs

TOOLPREFIX = riscv64-unknown-elf-
CXX = $(TOOLPREFIX)g++
AS = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
PY = python3
GDB = $(TOOLPREFIX)gdb
CP = cp
BUILDDIR = build

CXX_SRCS = $(shell find $K/ -type f -name '*.cc')
AS_SRCS = $(shell find $K/ -type f -name '*.S')
CXX_OBJS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(CXX_SRCS))))
AS_OBJS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(AS_SRCS))))
OBJS = $(CXX_OBJS) $(AS_OBJS) third_party/ptmalloc3/libptmalloc3.a

HEADER_DEP = $(addsuffix .d, $(basename $(CXX_OBJS)))

INCLUDEFLAGS = -I$K

CXXFLAGS = -Wall -Wextra -Werror # lint
ifeq ($(DEBUG_MODE), 1)
CXXFLAGS += -Og -gdwarf-2 -fno-omit-frame-pointer -ggdb # debug
CXXFLAGS += -D MEMORY_DEBUG -D COROUTINE_TRACE
else
CXXFLAGS += -O2
endif

CXXFLAGS += -fcoroutines -std=c++20 -fno-exceptions -fno-rtti -D HANDLE_MEMORY_ALLOC_FAIL # coroutine
CXXFLAGS += -foptimize-sibling-calls 
CXXFLAGS += -D NCPU=$(CPUS) # cpu
CXXFLAGS += -MD
CXXFLAGS += -mcmodel=medany
CXXFLAGS += -ffreestanding -fno-common -nostdlib -lgcc -mno-relax
CXXFLAGS += -Wno-error=write-strings -Wno-write-strings
#CXXFLAGS += -freport-bug
CXXFLAGS += $(INCLUDEFLAGS)
#CXXFLAGS += -fstack-protector # stack protector
CXXFLAGS += $(shell $(CXX) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

LOG ?= error

ifeq ($(LOG), error)
CXXFLAGS += -D LOG_LEVEL_ERROR
else ifeq ($(LOG), warn)
CXXFLAGS += -D LOG_LEVEL_WARN
else ifeq ($(LOG), info)
CXXFLAGS += -D LOG_LEVEL_INFO
else ifeq ($(LOG), debug)
CXXFLAGS += -D LOG_LEVEL_DEBUG
else ifeq ($(LOG), trace)
CXXFLAGS += -D LOG_LEVEL_TRACE
endif

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CXX) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CXXFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CXX) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CXXFLAGS += -fno-pie -nopie
endif

# empty target
.FORCE:

LDFLAGS= -z max-page-size=4096

-include $(HEADER_DEP)

$(AS_OBJS): $(BUILDDIR)/$K/%.o : $K/%.S Makefile
	@mkdir -p $(@D)
	$(AS) $(CXXFLAGS) -c $< -o $@

$(CXX_OBJS): $(BUILDDIR)/$K/%.o : $K/%.cc  $(BUILDDIR)/$K/%.d Makefile
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HEADER_DEP): $(BUILDDIR)/$K/%.d : $K/%.cc Makefile
	@mkdir -p $(@D)
	@set -e; rm -f $@; $(CC) -MM $< $(INCLUDEFLAGS) > $@.$$$$; \
        sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
        rm -f $@.$$$$



build: build/kernel

build/kernel: $(OBJS) os/kernel.ld
	$(LD) $(LDFLAGS) -T os/kernel.ld -o $(BUILDDIR)/kernel $(OBJS)
	@echo 'Build kernel done'

clean:
	rm -rf $(BUILDDIR)




# BOARD
BOARD		?= qemu
# SBI			?= opensbi
# BOOTLOADER	:= ./bootloader/rustsbi-qemu.bin
BOOTLOADER	:= ./bootloader/fw_jump.bin

QEMU = qemu-system-riscv64
QEMUOPTS = \
	-nographic \
	-smp $(CPUS) \
	-machine virt \
	-bios $(BOOTLOADER) \
	-kernel build/kernel	\
	-drive file=$(F)/fs-copy.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0


run: build/kernel
	# $(CP) $(F)/empty.img $(F)/fs-copy.img
	$(QEMU) $(QEMUOPTS) | tee qemu.log

# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::15234"; \
	else echo "-s -p 15234"; fi)

debug: build/kernel .gdbinit
	# $(CP) $(F)/empty.img $(F)/fs-copy.img
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB) &
	sleep 1
	$(GDB)
