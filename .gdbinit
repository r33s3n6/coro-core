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
b __trace_panic

set ((uint64*)&__exception_occurred)[0] = 0
set ((uint64*)&__exception_occurred)[1] = 0
set ((uint64*)&__exception_occurred)[2] = 0
set ((uint64*)&__exception_occurred)[3] = 0

cont
cont
cont
