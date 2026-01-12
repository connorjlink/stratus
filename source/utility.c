// Stratus: utility.c
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "utility.h"

static inline void platform_putchar(char c)
{
    /* UART0 on QEMU virt */
    volatile unsigned char* thr = (volatile unsigned char*)0x10000000u;
    volatile unsigned char* lsr = (volatile unsigned char*)0x10000005u;

    while (((*lsr) & (1u << 5)) == 0) { }
    *thr = (unsigned char)c;
}

size_t max(size_t x, size_t y)
{
    return x > y ? x : y;
}

size_t min(size_t x, size_t y)
{
    return y > x ? x : y;
}

size_t strlen(const char* str)
{
    size_t len = 0;

    while (str[len])
    {
        len++;
    }

    return len;
}

char* strcpy(char* destination, const char* string)
{
    char* result = destination;
    
    while (*string)
    {
        *destination++ = *string++;
    }

    *destination = '\0';
    return result;
}

int strcmp(const char* string1, const char* string2)
{
    while (*string1 && *string2)
    {
        if (*string1 != *string2)
        {
            break;
        }

        string1++;
        string2++;
    }

    return *(uint8_t*)string1 - *(uint8_t*)string2;
}

void* memset(void* pointer, unsigned char value, size_t number)
{
    uint8_t* p = (unsigned char*)pointer;

    while (number--)
    {
        *p++ = value;
    }

    return pointer;
}

void* memcpy(void* destination, const void* source, size_t number)
{
    uint8_t* d = (uint8_t*)destination;
    const uint8_t* s = (const uint8_t*)source;

    while (number--)
    {
        *d++ = *s++;
    }

    return destination;
}

void putchar(char c)
{
    if (c == '\n')
    {
        platform_putchar('\r');
    }

    platform_putchar(c);
}

void printf(const char* format, ...)
{
    va_list va;
    va_start(va, format);

    while (*format)
    {
        if (*format == '%')
        {
            format++;

            switch (*format)
            {
                case 'c':
                {
                    char c = (char)va_arg(va, int);
                    putchar(c);
                    break;
                }
                case 's':
                {
                    const char* str = va_arg(va, const char*);
                    while (*str)
                    {
                        putchar(*str++);
                    }
                    break;
                }
                case 'd':
                {
                    int number = va_arg(va, int);
                    char buffer[12];
                    int i = 0;
                    int is_negative = 0;

                    if (number < 0)
                    {
                        is_negative = 1;
                        number = -number;
                    }

                    do
                    {
                        buffer[i++] = (number % 10) + '0';
                        number /= 10;
                    } while (number > 0);

                    if (is_negative)
                    {
                        buffer[i++] = '-';
                    }

                    while (i--)
                    {
                        putchar(buffer[i]);
                    }

                    break;
                }
                case 'x':
                {
                    const unsigned int number = va_arg(va, unsigned int);
                    for (int i = (sizeof(unsigned int) * 2) - 1; i >= 0; i--)
                    {
                        unsigned char nibble = (number >> (i * 4)) & 0xF;
                        putchar("0123456789ABCDEF"[nibble]);
                    }
                }
                case '%':
                {
                    putchar('%');
                    break;
                }
                case '\0':
                {
                    putchar('%');
                    goto done;
                }
                default:
                    goto done;
            }
        }
        else
        {
            putchar(*format);
        }

        format++;
    }

    done:
        va_end(va);
}

