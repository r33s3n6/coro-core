// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE (0x00'8020'0000uLL)
#define IO_MEM_START (0x80000000 + 127 * 1024 * 1024)
#define PHYSTOP      (0x80000000 + 128 * 1024 * 1024) // 128M

#define VMEM_START (0x01'0000'0000uLL) // 4 GB, vmalloc start address
#define MMAP_START (0x20'0000'0000uLL) // 32 GB, mmap start address

// map the trampoline page to the highest address,
// in both user and kernel space.

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))  // 256 GB

#define KSTACK_SIZE (8192)
#define USTACK_SIZE (4096)
#define TRAPFRAME_SIZE (PGSIZE)

#define USER_TOP   (0x20'0000'0000uLL)  // 128 GB
#define TRAMPOLINE (MAXVA - PGSIZE)  // virtual address
#define TRAPFRAME  (TRAMPOLINE - TRAPFRAME_SIZE) // virtual address

#define USER_STACK_BOTTOM (USER_TOP) // 128 GB, user stack bottom address 
#define USER_STACK_TOP    (USER_TOP - USTACK_SIZE) // 128 GB - USTACK_SIZE, user stack top address
#define USER_TEXT_START   (0x00'0000'1000uLL)  // 4 KB, min user text start address

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L   // 256 MB
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L       // 192 MB
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)





extern char skernel[];
extern char ekernel[];

extern char s_text[];
extern char e_text[]; // kernel.ld sets this to end of kernel code.

extern char s_rodata[];
extern char e_rodata[];

extern char s_data[];
extern char e_data[];

extern char s_apps[];
extern char e_apps[];

extern char s_bss[];
extern char e_bss[];


extern char trampoline[];


extern char boot_stack_top[];
extern char boot_stack_bottom[];


extern char _entry[]; // kernel.ld sets this to the entry address of kernel code.

extern char trampoline[], user_vec[], user_ret[];

extern "C" void kernel_vec();
