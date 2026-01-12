// Stratus: kernel.c
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utility.h"
#include "defs.h"
#include "platform.h"
#include "fb_console.h"

#define COPYRIGHT_LOGO "STRATUS - (c) 2026 Connor J. Link. All Rights Reserved."

static uint8_t compose_color(VGAPalette palette)
{
    return (palette.bg << 4) | palette.fg;
}

static uint8_t invert_color(uint8_t color)
{
    return ((color & 0x0F) << 4) | ((color & 0xF0) >> 4);
}

static size_t g_term_cols = 80;
static size_t g_term_rows = 25;

void render_text_center(const char* data, size_t x0, size_t x1, size_t y)
{
    const size_t length = strlen(data);
    const size_t midpoint = (x0 + x1) / 2;
    
    const size_t begin = midpoint - (length / 2);
    terminal_write(data, length, begin, y);
}

void render_menubar()
{
    const size_t header_row = 0;
    const size_t copyright_row = (g_term_rows == 0) ? 0 : (g_term_rows - 1);

    _active_color = invert_color(_active_color);

    for (size_t x = 0; x < g_term_cols; x++)
    {
        terminal_putentryat(' ', _active_color, x, header_row);
        terminal_putentryat(' ', _active_color, x, copyright_row);
    }

    render_text_center("Configuration", 0, g_term_cols ? (g_term_cols - 1) : 0, header_row);
    render_text_center(COPYRIGHT_LOGO, 0, g_term_cols ? (g_term_cols - 1) : 0, copyright_row);

    _active_color = invert_color(_active_color);
}

void render_groupbox(Rect rect, uint8_t color, const char* title, bool is_selected)
{
    const size_t left = rect.pos.x;
    const size_t right = rect.pos.x + rect.size.x;

    const size_t top = rect.pos.y;
    const size_t bottom = rect.pos.y + rect.size.y;

    static const char horizontal_line = '\xC4';
    static const char vertical_line = '\xB3';

    static const char top_left_corner = '\xDA';
    static const char top_right_corner = '\xBF';
    static const char bottom_left_corner = '\xC0';
    static const char bottom_right_corner = '\xD9';

    // draw top line
    for (size_t x = left; x <= right; x++)
    {
        terminal_putentryat(horizontal_line, color, x, top);
    }

    // draw bottom line
    for (size_t x = left; x <= right; x++)
    {
        terminal_putentryat(horizontal_line, color, x, bottom);
    }

    // draw left line
    for (size_t y = top; y <= bottom; y++)
    {
        terminal_putentryat(vertical_line, color, left, y);
    }

    // draw right line
    for (size_t y = top; y <= bottom; y++)
    {
        terminal_putentryat(vertical_line, color, right, y);
    }

    // draw corners properly
    terminal_putentryat(top_left_corner, color, left, top);
    terminal_putentryat(top_right_corner, color, right, top);
    terminal_putentryat(bottom_left_corner, color, left, bottom);
    terminal_putentryat(bottom_right_corner, color, right, bottom);

    const size_t text_left = left + 2;
    const size_t text_right = right - 2;

    const size_t title_length = strlen(title);

    const size_t max_length = text_right - text_left;
    const int32_t difference = title_length - max_length;
    
    const size_t max_right = min(text_right, text_left + title_length - 1);

    if (is_selected)
    {
        color = invert_color(color);
    }

    for (size_t x = text_left; x <= max_right; x++)
    {
        terminal_putentryat(title[x - text_left], color, x, top);
    }

    if (difference > 0)
    {
        // terminal_putentryat(ellipses, color, text_right, top);
    }
}

void render_text(Rect parent, Point pos, uint8_t color, const char* text)
{
    const size_t start_left = parent.pos.x + pos.x + 1;
    const size_t start_top = parent.pos.y + pos.y + 1;

    const size_t parent_right = parent.pos.x + parent.size.x - 1;

    const size_t text_length = strlen(text);

    size_t x = start_left;
    size_t y = start_top;

    for (size_t i = 0; i < text_length; i++)
    {
        char c = text[i];

        if (x == parent_right ||
            c == '\n')
        {
            y++;
            x = start_left;
        }

        terminal_putentryat(c, color, x, y);

        x++;
    }
}

void render_text_justified(Rect parent, Point pos, uint8_t color, const char* text)
{
    const uint8_t color_cached = _active_color;
    _active_color = color;

    const size_t left = parent.pos.x + pos.x + 1;
    const size_t right = parent.pos.x + parent.size.x + pos.x;

    const size_t top = parent.pos.y + pos.y + 1;

    render_text_center(text, left, right, top);

    _active_color = color_cached;
}

void scroll_rect(Rect parent, uint8_t color)
{
    const size_t left = parent.pos.x + 1;
    const size_t right = parent.pos.x + parent.size.x;

    const size_t top = parent.pos.y + 1;
    const size_t bottom = parent.pos.y + parent.size.y - 1;

    for (size_t y = top; y < bottom; y++)
    {
        for (size_t x = left; x < right; x++)
        {
            char c;
            uint8_t ccolor;
            if (!terminal_getentryat(x, y + 1, &c, &ccolor))
            {
                c = ' ';
                ccolor = color;
            }

            terminal_putentryat(c, ccolor, x, y);
        }
    }

    for (size_t x = left; x < right; x++)
    {
        terminal_putentryat(' ', color, x, bottom);
    }
}

void erase_rect(Rect rect, uint8_t color)
{
    const size_t left = rect.pos.x + 1;
    const size_t right = rect.pos.x + rect.size.x;

    const size_t top = rect.pos.y + 1;
    const size_t bottom = rect.pos.y + rect.size.y - 1;

    for (int i = left; i < right; i++)
    {
        for (int j = top; j < bottom; j++)
        {
            terminal_putentryat(' ', color, i, j);
        }
    }
}

void write_console(Rect parent, Point* cursor, uint8_t color, const char* text)
{
    const size_t bottom = parent.size.y - 2;

    if (cursor->y == bottom)
    {
        scroll_rect(parent, color);
        //cursor->y--;
    }

    else
    {
        cursor->y++;
    }

    render_text(parent, *cursor, color, text);
}

void trap_exception_handler(uint32_t scause, uint32_t sepc, uint32_t stval)
{
    printf("TRAP: scause=0x%x sepc=0x%x stval=0x%x\n", (unsigned)scause, (unsigned)sepc, (unsigned)stval);
    for (;;)
    {
#ifdef __GNUC__
        __asm__ volatile ("wfi");
#endif
    }
}

const char* _explorer_items[] =
{
    "Editor",
    "Terminal",
    "Settings",
    "About",
};

bool _explorer_selected = true;
size_t _explorer_index = 0;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

void render_explorer()
{
    Point point = POINT(0, 0);

    for (size_t i = 0; i < ARRAY_SIZE(_explorer_items); i++)
    {
        uint8_t color = _active_color;

        if (_explorer_selected && _explorer_index == i)
        {
            color = invert_color(color);
        }

        render_text(_explorer_rect, point, color, _explorer_items[i]);
        point.y++;
    }
}

void render_editor() 
{
    erase_rect(_navigator_rect, _active_color);
    render_text(_navigator_rect, POINT(0, 1), _active_color, "EDITOR");
}

void render_terminal()
{
    erase_rect(_navigator_rect, _active_color);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_color, "TERMINAL");
}

void render_settings()
{
    erase_rect(_navigator_rect, _active_color);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_color, "SETTINGS");
}

void render_about()
{
    erase_rect(_navigator_rect, _active_color);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_color, "ABOUT");
    render_text_justified(_navigator_rect, POINT(0, 3), _active_color, COPYRIGHT_LOGO);
}

static void render_active_view(void)
{
    switch (_explorer_index)
    {
        case 0:
            render_editor();
            break;
        case 1:
            render_terminal();
            break;
        case 2:
            render_settings();
            break;
        case 3:
            render_about();
            break;
        default:
            break;
    }
}

static void type_backspace(size_t* x, size_t* y)
{
    if (!x || !y)
    {
        return;
    }

    if (*x == 0)
    {
        return;
    }

    (*x)--;
    terminal_putentryat(' ', _active_color, *x, *y);
}

void kernel_main(void)
{
    printf("kernel: enter\n");
    terminal_initialize();

    terminal_get_size(&g_term_cols, &g_term_rows);
    layout_init(g_term_cols, g_term_rows);

    printf("kernel: terminal_initialize returned\n");
    render_menubar();
    terminal_flush();

    size_t x = 43;
    size_t y = 34;

    render_groupbox(_explorer_rect, _active_color, "Explorer", false);
    render_groupbox(_console_rect, _active_color, "Console", false);
    render_groupbox(_navigator_rect, _active_color, "Navigator", false);

    render_explorer();	
    terminal_flush();

    while (1)
    {
        KeyboardEvent ev;
        if (keyboard_poll_event(&ev))
        {
            const bool is_key = (ev.type == KBD_EV_KEY);
            const bool is_press = (ev.value == 1 || ev.value == 2);

            if (is_key && is_press)
            {
                switch (ev.code)
                {
                    case KBD_KEY_UP:
                    {
                        if (_explorer_selected && _explorer_index > 0)
                        {
                            _explorer_index--;
                            render_explorer();
                        }
                    } break;

                    case KBD_KEY_DOWN:
                    {
                        if (_explorer_selected && _explorer_index < ARRAY_SIZE(_explorer_items) - 1)
                        {
                            _explorer_index++;
                            render_explorer();
                        }
                    } break;

                    case KBD_KEY_RIGHT:
                    {
                        if (_explorer_selected)
                        {
                            _explorer_selected = false;
                            render_explorer();
                            render_active_view();
                        }
                    } break;

                    case KBD_KEY_LEFT:
                    {
                        if (!_explorer_selected)
                        {
                            _explorer_selected = true;
                            render_explorer();
                        }
                    } break;

                    case KBD_KEY_ENTER:
                    {
                        // Enter activates the current selection.
                        if (_explorer_selected)
                        {
                            _explorer_selected = false;
                            render_explorer();
                        }
                        render_active_view();
                    } break;

                    case KBD_KEY_BACKSPACE:
                    {
                        type_backspace(&x, &y);
                    } break;

                    default:
                    {
                        // Non-navigation key: treat as typed character if representable.
                        if (ev.ascii)
                        {
                            if (ev.ascii == 'q')
                            {
                                shut_down();
                            }
                            else if (ev.ascii == '\b')
                            {
                                type_backspace(&x, &y);
                            }
                            else
                            {
                                terminal_putchar(ev.ascii, &x, &y);
                            }
                        }
                    } break;
                }
            }

            terminal_flush();
        }
    }
}
