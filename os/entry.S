    .section .text.entry
    .globl _entry
    .globl shutdown
_entry:
    .cfi_startproc
    # a0: hartid
    # every core has a boot stack of 4 KB
    la   sp, boot_stack_top
    li   t1, 1024 * 4   # t1 = 4KB
    addi t0, a0, 1    # t0 = hartid + 1
    mul  t0, t0, t1    # 4K * (hartid + 1)
    add  sp, sp, t0  
    mv   fp, x0        # fp = 0
    mv   tp, a0        # set hart id: tp = hartid 
    call kernel_init

    call shutdown
    .cfi_endproc

    .section .bss.stack
    .globl boot_stack_top
boot_stack_top:
    .space 1024 * 4 * 8
    .globl boot_stack_bottom
boot_stack_bottom:

