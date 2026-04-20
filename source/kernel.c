#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utility.h"
#include "defs.h"
#include "keyboard.h"
#include "console.h"

// Stratus: kernel.c
// (c) Connor J. Link. All Rights Reserved.

#define COPYRIGHT_LOGO "STRATUS - (c) Connor J. Link. All Rights Reserved."

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
    const size_t copyright_row = (_rows == 0) ? 0 : (_rows - 1);

    _active_palette = INVERT_PALETTE(_active_palette);

    for (size_t x = 0; x < _columns; x++)
    {
        terminal_putentryat(' ', _active_palette, x, header_row);
        terminal_putentryat(' ', _active_palette, x, copyright_row);
    }

    render_text_center("Configuration", 0, _columns ? (_columns - 1) : 0, header_row);
    render_text_center(COPYRIGHT_LOGO, 0, _columns ? (_columns - 1) : 0, copyright_row);

    _active_palette = INVERT_PALETTE(_active_palette);
}

void render_groupbox(Rectangle Rectangle, Palette palette, const char* title, bool is_selected)
{
    const size_t left = Rectangle.position.x;
    const size_t right = Rectangle.position.x + Rectangle.size.x;

    const size_t top = Rectangle.position.y;
    const size_t bottom = Rectangle.position.y + Rectangle.size.y;

    static const char horizontal_line = '\xC4';
    static const char vertical_line = '\xB3';

    static const char top_left_corner = '\xDA';
    static const char top_right_corner = '\xBF';
    static const char bottom_left_corner = '\xC0';
    static const char bottom_right_corner = '\xD9';

    // draw top line
    for (size_t x = left; x <= right; x++)
    {
        terminal_putentryat(horizontal_line, palette, x, top);
    }

    // draw bottom line
    for (size_t x = left; x <= right; x++)
    {
        terminal_putentryat(horizontal_line, palette, x, bottom);
    }

    // draw left line
    for (size_t y = top; y <= bottom; y++)
    {
        terminal_putentryat(vertical_line, palette, left, y);
    }

    // draw right line
    for (size_t y = top; y <= bottom; y++)
    {
        terminal_putentryat(vertical_line, palette, right, y);
    }

    // draw corners properly
    terminal_putentryat(top_left_corner, palette, left, top);
    terminal_putentryat(top_right_corner, palette, right, top);
    terminal_putentryat(bottom_left_corner, palette, left, bottom);
    terminal_putentryat(bottom_right_corner, palette, right, bottom);

    const size_t text_left = left + 2;
    const size_t text_right = right - 2;

    const size_t title_length = strlen(title);

    const size_t max_length = text_right - text_left;
    const int32_t difference = title_length - max_length;
    
    const size_t max_right = min(text_right, text_left + title_length - 1);

    if (is_selected)
    {
        palette = INVERT_PALETTE(palette);
    }

    for (size_t x = text_left; x <= max_right; x++)
    {
        terminal_putentryat(title[x - text_left], palette, x, top);
    }

    if (difference > 0)
    {
        // terminal_putentryat(ellipses, color, text_right, top);
    }
}

void render_text(Rectangle parent, Point position, Palette palette, const char* text)
{
    const size_t start_left = parent.position.x + position.x + 1;
    const size_t start_top = parent.position.y + position.y + 1;

    const size_t parent_right = parent.position.x + parent.size.x - 1;

    const size_t text_length = strlen(text);

    size_t x = start_left;
    size_t y = start_top;

    for (size_t i = 0; i < text_length; i++)
    {
        uint8_t ascii = text[i];

        if (x == parent_right ||
            ascii == '\n')
        {
            y++;
            x = start_left;
        }

        terminal_putentryat(ascii, palette, x, y);

        x++;
    }
}

void render_text_justified(Rectangle parent, Point position, Palette palette, const char* text)
{
    const Palette palette_cached = _active_palette;
    _active_palette = palette;

    const size_t left = parent.position.x + position.x + 1;
    const size_t right = parent.position.x + parent.size.x + position.x;

    const size_t top = parent.position.y + position.y + 1;

    render_text_center(text, left, right, top);

    _active_palette = palette_cached;
}

void scroll_rect(Rectangle parent, Palette palette)
{
    const size_t left = parent.position.x + 1;
    const size_t right = parent.position.x + parent.size.x;

    const size_t top = parent.position.y + 1;
    const size_t bottom = parent.position.y + parent.size.y - 1;

    for (size_t y = top; y < bottom; y++)
    {
        for (size_t x = left; x < right; x++)
        {
            uint8_t ascii;
            Palette cpalette;

            if (!terminal_getentryat(x, y + 1, &ascii, &cpalette))
            {
                ascii = ' ';
                cpalette = palette;
            }

            terminal_putentryat(ascii, cpalette, x, y);
        }
    }

    for (size_t x = left; x < right; x++)
    {
        terminal_putentryat(' ', palette, x, bottom);
    }
}

static void erase_rect(Rectangle Rectangle, Palette palette)
{
    const size_t left = Rectangle.position.x + 1;
    const size_t right = Rectangle.position.x + Rectangle.size.x;

    const size_t top = Rectangle.position.y + 1;
    const size_t bottom = Rectangle.position.y + Rectangle.size.y - 1;

    for (int i = left; i < right; i++)
    {
        for (int j = top; j < bottom; j++)
        {
            terminal_putentryat(' ', palette, i, j);
        }
    }
}

static void write_console(Rectangle parent, Point* cursor, Palette palette, const char* text)
{
    const size_t bottom = parent.size.y - 2;

    if (cursor->y == bottom)
    {
        scroll_rect(parent, palette);
    }
    else
    {
        cursor->y++;
    }

    render_text(parent, *cursor, palette, text);
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
        Palette palette = _active_palette;

        if (_explorer_selected && _explorer_index == i)
        {
            palette = INVERT_PALETTE(palette);
        }

        render_text(_explorer_rect, point, palette, _explorer_items[i]);
        point.y++;
    }
}

void render_editor() 
{
    erase_rect(_navigator_rect, _active_palette);
    render_text(_navigator_rect, POINT(0, 1), _active_palette, "EDITOR");
}

void render_terminal()
{
    erase_rect(_navigator_rect, _active_palette);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_palette, "TERMINAL");
}

void render_settings()
{
    erase_rect(_navigator_rect, _active_palette);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_palette, "SETTINGS");
}

void render_about()
{
    erase_rect(_navigator_rect, _active_palette);
    render_text_justified(_navigator_rect, POINT(0, 1), _active_palette, "ABOUT");
    render_text_justified(_navigator_rect, POINT(0, 3), _active_palette, COPYRIGHT_LOGO);
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
    terminal_putentryat(' ', _active_palette, *x, *y);
}

void kernel_main(void)
{
    printf("kernel: enter\n");
    terminal_initialize();

    terminal_get_size(&_columns, &_rows);
    layout_init(_columns, _rows);

    render_menubar();
    terminal_flush();

    size_t x = 43;
    size_t y = 34;

    render_groupbox(_explorer_rect, _active_palette, "Explorer", false);
    render_groupbox(_console_rect, _active_palette, "Console", false);
    render_groupbox(_navigator_rect, _active_palette, "Navigator", false);

    render_explorer();	
    terminal_flush();

    while (1)
    {
        KeyboardEvent event;
        if (keyboard_poll_event(&event))
        {
            const bool is_key = (event.type == KBD_EV_KEY);
            const bool is_press = (event.value == 1 || event.value == 2);

            if (is_key && is_press)
            {
                switch (event.code)
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
                        if (event.c)
                        {
                            if (event.c == 'q')
                            {
                                void shut_down(void);
                                shut_down();
                            }
                            else if (event.c == '\b')
                            {
                                type_backspace(&x, &y);
                            }
                            else
                            {
                                terminal_putchar(event.c, &x, &y);
                            }
                        }
                    } break;
                }
            }

            terminal_flush();
        }
    }
}
