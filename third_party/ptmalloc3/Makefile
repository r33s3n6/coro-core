# Makefile for ptmalloc, version 3
# by Wolfram Gloger 1996-1999, 2001-2005, 2006


CC = riscv64-unknown-elf-gcc

SYS_FLAGS  = -ffreestanding -fno-common -nostdlib -lgcc -mno-relax -MD -mcmodel=medany
OPT_FLAGS  = -g -O2
WARN_FLAGS = -Wall -Wstrict-prototypes

INC_FLAGS  = -Isysdeps/generic -I.

# Flags for the compilation of malloc.c
M_FLAGS   = -DTHREAD_STATS=1 


RM        = rm -f
AR        = ar
RANLIB    = ranlib

MALLOC_OBJ = ptmalloc3.o malloc.o
LIB_MALLOC = libptmalloc3.a


CFLAGS = $(SYS_FLAGS) $(OPT_FLAGS) $(WARN_FLAGS) $(INC_FLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $<

all: $(LIB_MALLOC)

ptmalloc3.o: ptmalloc3.c malloc-2.8.3.h
	$(CC) -c $(CFLAGS) $(M_FLAGS) -DMSPACES=1 $<

malloc.o: malloc.c
	$(CC) -c $(CFLAGS) $(M_FLAGS) -DONLY_MSPACES -DUSE_LOCKS=0 $<

libptmalloc3.a: $(MALLOC_OBJ)
	$(AR) cr $@ $(MALLOC_OBJ)
	$(RANLIB) $@



clean:
	$(RM) $(MALLOC_OBJ) libptmalloc3.a libptmalloc3.so \
         core core.[0-9]*




# dependencies
ptmalloc3.o: malloc-private.h
