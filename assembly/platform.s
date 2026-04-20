// Stratus: platform.s
// (c) Connor J. Link. All Rights Reserved.
    
    .section .rodata
trap_format_string:
    .string "TRAP: scause=0x%x sepc=0x%x stval=0x%x\n"

    .section .text
    
    .globl shut_down
shut_down:
    li a7, 8
    ecall a7
hang:
    wfi
    j hang


    .globl read_timestamp
read_timestamp:
    csrr a0, time
    ret


    .globl trap_handler
trap_exception_handler:
    addi sp, sp, -16
    sw ra, 12(sp)

    mv t0, a0 # scause
    mv t1, a1 # sepc
    mv t2, a2 # stval

    la a0, trap_format_string
    mv a1, t0 # scause
    mv a2, t1 # sepc
    mv a3, t2 # stval

    call printf

hang:
    wfi
    j hang

