// Stratus: platform.c
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>

#define UART0_BASE 0x10000000u
#define UART_RHR   0x00u
#define UART_THR   0x00u
#define UART_LSR   0x05u

static inline uint8_t mmio8(uint32_t address)
{
    return *(volatile uint8_t*)address;
}

static inline void mmio8_write(uint32_t address, uint8_t v)
{
    *(volatile uint8_t*)address = v;
}

static inline void uart_putchar(char c)
{
    while ((mmio8(UART0_BASE + UART_LSR) & (1u << 5)) == 0)
    {
    }

    mmio8_write(UART0_BASE + UART_THR, (uint8_t)c);
}

static inline int uart_getchar_nonblock(void)
{
    if ((mmio8(UART0_BASE + UART_LSR) & 1u) == 0)
    {
        return -1;
    }

    return (int)mmio8(UART0_BASE + UART_RHR);
}

static inline void sbi_shutdown_legacy(void)
{
    register uint32_t a7 asm("a7") = 8;
    asm volatile ("ecall" : : "r"(a7) : "memory");
    for (;;) { asm volatile ("wfi"); }
}

char poll_keyboard(void)
{
    int c = uart_getchar_nonblock();
    return (c < 0) ? '\0' : (char)c;
}

void shut_down(void)
{
    sbi_shutdown_legacy();
}

void restart(void)
{
    /* If you want reboot, implement SBI SRST (v0.2) later; for now shutdown */
    sbi_shutdown_legacy();
}

int read_timestamp(void)
{
    /* rdtime is 64-bit; return low 32 for now */
    uint32_t lo;
    asm volatile ("rdtime %0" : "=r"(lo));
    return (int)lo;
}
