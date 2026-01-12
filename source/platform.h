#ifndef STRATUS_PLATFORM_H
#define STRATUS_PLATFORM_H

// Stratus: platform.h
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    KMOD_SHIFT = 1u << 0,
    KMOD_CTRL  = 1u << 1,
    KMOD_ALT   = 1u << 2,
    KMOD_META  = 1u << 3,
} KeyModifiers;

typedef struct
{
    uint16_t type;
    uint16_t code;
    int32_t value;
    uint32_t modifiers;
    char ascii;
} KeyboardEvent;

// Keyboard event constants (subset of Linux input-event / key codes)
#define KBD_EV_KEY 1u

#define KBD_KEY_ESC       1u
#define KBD_KEY_ENTER     28u
#define KBD_KEY_BACKSPACE 14u

#define KBD_KEY_UP        103u
#define KBD_KEY_LEFT      105u
#define KBD_KEY_RIGHT     106u
#define KBD_KEY_DOWN      108u

char poll_keyboard(void);
bool keyboard_poll_event(KeyboardEvent* out_event);
void shut_down(void);
void restart(void);
int read_timestamp(void);
void trap_exception_handler(uint32_t scause, uint32_t sepc, uint32_t stval);

#define READ_CSR(reg)                      \
    do {                                   \
        unsigned int value;                \
        asm volatile ("mov %%" #reg ", %0" \
                      : "=r"(value)        \
                      :                    \
        );                                 \
        return value;                      \
    } while (0)

#define WRITE_CSR(reg, value)  \
    do {                       \
        asm volatile (         \
            "mov %0, %%" #reg  \
            :                  \
            : "r"(value)       \
        );                     \
    } while (0)

#endif
