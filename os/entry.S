# declare boot stack in bss section
# and call kernel_init to start kernel
    .section .text.entry
    .globl _entry
_entry:
    la sp, boot_stack_top
    call kernel_init

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 * 16
    .globl boot_stack_top
boot_stack_top: