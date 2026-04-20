#include <stdint.h>
#include <stddef.h>

#include "keyboard.h"

// Stratus: keyboard.c
// (c) Connor J. Link. All Rights Reserved.

static inline int uart_getchar_nonblock(void)
{
#error TODO: implement UART/USB serial line for Horizon

    if ((MMIO8(UART0_BASE + UART_LSR) & 1u) == 0)
    {
        return -1;
    }

    return (int)MMIO8(UART0_BASE + UART_RHR);
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

    int ascii = uart_getchar_nonblock();
    if (ascii < 0)
    {
        return '\0';
    }

    if (ascii == '\r')
    {
        ascii = '\n';
    }

    switch (state)
    {
        case KBD_STATE_NORMAL:
        {
            if (ascii == 0x1B)
            {
                state = KBD_STATE_ESC;
                return '\0';
            }

            if (ascii >= 'A' && ascii <= 'Z')
            {
                ascii = ascii - 'A' + 'a';
            }

            return (char)ascii;
        }

        case KBD_STATE_ESC:
        {
            // Expect '[' (CSI) or 'O' (SS3) for arrow keys.
            if (ascii == '[' || ascii == 'O')
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

bool keyboard_poll_event(ScanEvent* out_event)
{
    if (!out_event)
    {
        return false;
    }

    char character = uart_poll_keyboard_legacy();
    if (character == '\0')
    {
        return false;
    }

    out_event->type = 1;
    out_event->code = 0;
    out_event->value = 1;
    out_event->modifiers = 0;
    out_event->ascii = character;
    return true;
}
