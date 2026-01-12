OUTPUT_ARCH(riscv)
ENTRY(_start)

/* QEMU virt RAM at 0x80000000.
   Payload linked at 0x80200000. */
MEMORY
{
  RAM (rwx) : ORIGIN = 0x80200000, LENGTH = 128M - 2M
}

SECTIONS
{
  . = ORIGIN(RAM);

  .text : ALIGN(4K)
  {
    KEEP(*(.init))
    *(.text .text.*)
  } > RAM

  .rodata : ALIGN(4K)
  {
    *(.rodata .rodata.*)
  } > RAM

  .data : ALIGN(4K)
  {
    *(.data .data.*)
  } > RAM

  .bss (NOLOAD) : ALIGN(16)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > RAM

  /* Simple fixed stack at the end of the RAM region */
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);

  /* If you later use small-data sections, set gp near them */
  __global_pointer$ = . + 0x800;
}