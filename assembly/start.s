    .section .init
    .globl _start
    .option norvc

_start:
    /* OpenSBI enters in S-mode with a0 = hartid, a1 = dtb pointer */

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