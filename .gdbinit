set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:15234
symbol-file build/kernel
# set disassemble-next-line auto
display/12i $pc-8
set riscv use-compressed-breakpoints yes

# entry point
break *0x1000
# kernel exception trace
b __trace_exception
b __trace_exception__sret
b __early_trace_exception
b __trace_panic

cont
cont
cont
