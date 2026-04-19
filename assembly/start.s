    .section .init
    .globl _start
    .option norvc

_start:
    # OpenSBI enters in S-mode with a0 = hartid, a1 = dtb pointer
    # install a minimal trap vector so faults don't fault and reboot
    la t0, trap_vector
    csrw stvec, t0

    la sp, __stack_top

    la t0, __bss_start
    la t1, __bss_end
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:
    call kernel_main

3:
    wfi
    j 3b

    .align 4
trap_vector:
    addi sp, sp, -16
    sw ra, 12(sp)
    sw a0, 8(sp)
    sw a1, 4(sp)
    sw a2, 0(sp)

    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    call trap_exception_handler

trap_hang:
    wfi
    j trap_hang
