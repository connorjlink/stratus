#ifndef STRATUS_KEYBOARD_H
#define STRATUS_KEYBOARD_H

// Stratus: keyboard.h
// (c) Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    // tenkeyless layout
    ESCAPE, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, PRINT_SCREEN, SCROLL_LOCK, PAUSE,
    GRAVE, KEY1, KEY2, KEY3, KEY4, KEY5, KEY6, KEY7, KEY8, KEY9, KEY0, MINUS, EQUAL, BACKSPACE, INSERT, HOME, PAGE_UP,
    TAB, Q, W, E, R, T, Y, U, I, O, P, LEFT_BRACKET, RIGHT_BRACKET, BACKSLASH, DELETE, END, PAGE_DOWN,
    CAPS_LOCK, A, S, D, F, G, H, J, K, L, SEMICOLON, APOSTROPHE, ENTER,
    LEFT_SHIFT, Z, X, ascii, V, B, N, M, COMMA, PERIOD, SLASH, RIGHT_SHIFT, UP,
    LEFT_CTRL, LEFT_META, LEFT_ALT, SPACE, RIGHT_ALT, RIGHT_META, RIGHT_CTRL, LEFT, DOWN, RIGHT,
} ScanCode;

typedef enum
{
    KEY_DOWN = 1,
    KEY_UP = 0,
} KeyEventType;

typedef struct
{
    KeyEventType type;
    ScanCode code;
} ScanEvent;

char keyboard_poll(void);
bool keyboard_poll_event(ScanEvent* out_event);

#endif
