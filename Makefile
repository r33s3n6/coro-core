.PHONY: clean build user run debug test .FORCE
all: build

K = os
#U = user
#F = nfs

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
OBJS = $(CXX_OBJS) $(AS_OBJS)

HEADER_DEP = $(addsuffix .d, $(basename $(CXX_OBJS)))

INCLUDEFLAGS = -I$K

CXXFLAGS = -Wall -Werror # lint
CXXFLAGS += -Og -g -fno-omit-frame-pointer -ggdb # debug
CXXFLAGS += -foptimize-sibling-calls -fcoroutines -std=c++20 -fno-exceptions -fno-rtti # coroutine
CXXFLAGS += -MD
CXXFLAGS += -mcmodel=medany
CXXFLAGS += -ffreestanding -fno-common -nostdlib -lgcc -mno-relax
CXXFLAGS += -Wno-error=write-strings -Wno-write-strings
CXXFLAGS += $(INCLUDEFLAGS)
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

$(AS_OBJS): $(BUILDDIR)/$K/%.o : $K/%.S
	@mkdir -p $(@D)
	$(AS) $(CFLAGS) -c $< -o $@

$(CXX_OBJS): $(BUILDDIR)/$K/%.o : $K/%.cc  $(BUILDDIR)/$K/%.d
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HEADER_DEP): $(BUILDDIR)/$K/%.d : $K/%.cc
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
SBI			?= rustsbi
BOOTLOADER	:= ./bootloader/rustsbi-qemu.bin

QEMU = qemu-system-riscv64
QEMUOPTS = \
	-nographic \
	-machine virt \
	-bios $(BOOTLOADER) \
	-kernel build/kernel#	\
	#-drive file=$(F)/fs-copy.img,if=none,format=raw,id=x0 \
    #-device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0


run: build/kernel
	$(QEMU) $(QEMUOPTS)