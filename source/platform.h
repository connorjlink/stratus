#ifndef STRATUS_PLATFORM_H
#define STRATUS_PLATFORM_H

// Stratus: platform.h
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>

char poll_keyboard(void);
void shut_down(void);
void restart(void);
int read_timestamp(void);
void exception_handler(uint64_t code);

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
