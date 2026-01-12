// Stratus: platform.c
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stddef.h>

#include "platform.h"
#include "virtio_input.h"

#define UART0_BASE ((uintptr_t)0x10000000u)
#define UART_RHR   0x00u
#define UART_THR   0x00u
#define UART_LSR   0x05u

static inline uint8_t mmio8(uintptr_t address)
{
    return *(volatile uint8_t*)address;
}

static inline void mmio8_write(uintptr_t address, uint8_t v)
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
#ifdef __GNUC__
    register uint32_t a7 __asm__("a7") = 8;
    __asm__ volatile ("ecall" : : "r"(a7) : "memory");
    for (;;) { __asm__ volatile ("wfi"); }
#else
    for (;;) { }
#endif
}

static char uart_poll_keyboard_legacy(void)
{
    enum
    {
        KBD_STATE_NORMAL = 0,
        KBD_STATE_ESC,
        KBD_STATE_CSI,
    };

    static int state = KBD_STATE_NORMAL;

    int c = uart_getchar_nonblock();
    if (c < 0)
    {
        return '\0';
    }

    if (c == '\r')
    {
        c = '\n';
    }

    switch (state)
    {
        case KBD_STATE_NORMAL:
        {
            if (c == 0x1B)
            {
                state = KBD_STATE_ESC;
                return '\0';
            }

            if (c >= 'A' && c <= 'Z')
            {
                c = c - 'A' + 'a';
            }

            return (char)c;
        }

        case KBD_STATE_ESC:
        {
            // Expect '[' (CSI) or 'O' (SS3) for arrow keys.
            if (c == '[' || c == 'O')
            {
                state = KBD_STATE_CSI;
                return '\0';
            }

            // Unknown escape sequence; reset.
            state = KBD_STATE_NORMAL;
            return '\0';
        }

        case KBD_STATE_CSI:
        {
            state = KBD_STATE_NORMAL;

            switch (c)
            {
                case 'A': // Up
                case 'B': // Down
                case 'C': // Right
                case 'D': // Left
                    // Do not alias arrow keys to WASD.
                    return '\0';
                default:
                    return '\0';
            }
        }

        default:
            state = KBD_STATE_NORMAL;
            return '\0';
    }
}

bool keyboard_poll_event(KeyboardEvent* output_event)
{
    if (!output_event)
    {
        return false;
    }

    static bool tried_virtio;
    static bool have_virtio;

    if (!tried_virtio)
    {
        tried_virtio = true;
        have_virtio = virtio_keyboard_init();
    }

    if (have_virtio)
    {
        if (virtio_keyboard_poll_event(output_event))
        {
            return true;
        }
    }

    char character = uart_poll_keyboard_legacy();
    if (character == '\0')
    {
        return false;
    }

    output_event->type = 1;
    output_event->code = 0;
    output_event->value = 1;
    output_event->modifiers = 0;
    output_event->ascii = character;
    return true;
}

char poll_keyboard(void)
{
    KeyboardEvent ev;
    if (keyboard_poll_event(&ev) && ev.ascii != 0)
    {
        return ev.ascii;
    }

    return '\0';
}

void shut_down(void)
{
    sbi_shutdown_legacy();
}

void restart(void)
{
    sbi_shutdown_legacy();
}

int read_timestamp(void)
{
#ifdef __GNUC__
    uint32_t low;
    __asm__ volatile ("rdtime %0" : "=r"(low));
    return (int)low;
#else
    return 0;
#endif
}
