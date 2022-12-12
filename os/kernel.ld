OUTPUT_ARCH(riscv)
ENTRY(_entry)
BASE_ADDRESS = 0x80200000;

SECTIONS
{
    . = BASE_ADDRESS;
    skernel = .;

    s_text = .;
    .text : {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(0x1000);
        
        *(trampsec)
        . = ALIGN(0x1000);
        
    }

    . = ALIGN(4K);
    e_text = .;
    s_rodata = .;
    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4K);
    e_rodata = .;
    s_data = .;
    .data : {
        s_apps = .;
        *(.data.apps)
        e_apps = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    . = ALIGN(4K);
    e_data = .;
    .bss : {
        *(.bss.stack)
        s_bss = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }

    . = ALIGN(4K);
    e_bss = .;
    .preinit_array :   
    {
      PROVIDE_HIDDEN (__preinit_array_start = .); 
      KEEP (*allocator.o (.init_array .ctors))
      KEEP (*(.preinit_array))
      PROVIDE_HIDDEN (__preinit_array_end = .); 
    }
    .init_array :   
    {
      PROVIDE_HIDDEN (__init_array_start = .); 
      KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
      KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o *allocator.o) .ctors))
      PROVIDE_HIDDEN (__init_array_end = .); 
    }
    .fini_array :   
    {
      PROVIDE_HIDDEN (__fini_array_start = .); 
      KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
      KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
      PROVIDE_HIDDEN (__fini_array_end = .); 
    }
    ekernel = .;

    .eh_frame : {
        *(.eh_frame)
    }
}