#include "virtio_input.h"

#include "memory.h"
#include "utility.h"
#include "virtio_mmio.h"

// virtio-input device id
#define VIRTIO_DEVICE_ID_INPUT 18u

// virtio-mmio status register + bits (duplicated here for simplicity)
#define VIRTIO_MMIO_STATUS 0x070u
#define VIRTIO_STATUS_DRIVER_OK 4u

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off)
{
    return *(volatile uint32_t*)(base + off);
}

static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t v)
{
    *(volatile uint32_t*)(base + off) = v;
}

static inline void fence_iorw(void)
{
#if defined(__riscv)
    __asm__ volatile ("fence iorw, iorw" : : : "memory");
#else
    (void)0;
#endif
}

// Linux input event types
#define EV_SYN 0x00u
#define EV_KEY 0x01u

// A small subset of Linux input key codes (enough for menu + typing)
#define KEY_ESC         1u
#define KEY_1           2u
#define KEY_2           3u
#define KEY_3           4u
#define KEY_4           5u
#define KEY_5           6u
#define KEY_6           7u
#define KEY_7           8u
#define KEY_8           9u
#define KEY_9           10u
#define KEY_0           11u
#define KEY_MINUS       12u
#define KEY_EQUAL       13u
#define KEY_BACKSPACE   14u
#define KEY_TAB         15u
#define KEY_Q           16u
#define KEY_W           17u
#define KEY_E           18u
#define KEY_R           19u
#define KEY_T           20u
#define KEY_Y           21u
#define KEY_U           22u
#define KEY_I           23u
#define KEY_O           24u
#define KEY_P           25u
#define KEY_LEFTBRACE   26u
#define KEY_RIGHTBRACE  27u
#define KEY_ENTER       28u
#define KEY_LEFTCTRL    29u
#define KEY_A           30u
#define KEY_S           31u
#define KEY_D           32u
#define KEY_F           33u
#define KEY_G           34u
#define KEY_H           35u
#define KEY_J           36u
#define KEY_K           37u
#define KEY_L           38u
#define KEY_SEMICOLON   39u
#define KEY_APOSTROPHE  40u
#define KEY_GRAVE       41u
#define KEY_LEFTSHIFT   42u
#define KEY_BACKSLASH   43u
#define KEY_Z           44u
#define KEY_X           45u
#define KEY_C           46u
#define KEY_V           47u
#define KEY_B           48u
#define KEY_N           49u
#define KEY_M           50u
#define KEY_COMMA       51u
#define KEY_DOT         52u
#define KEY_SLASH       53u
#define KEY_RIGHTSHIFT  54u
#define KEY_LEFTALT     56u
#define KEY_SPACE       57u
#define KEY_CAPSLOCK    58u

#define KEY_RIGHTCTRL   97u
#define KEY_RIGHTALT    100u
#define KEY_LEFT        105u
#define KEY_RIGHT       106u
#define KEY_DOWN        108u
#define KEY_UP          103u

#define KEY_LEFTMETA    125u
#define KEY_RIGHTMETA   126u

// Virtio input event layout
typedef struct
{
    uint16_t type;
    uint16_t code;
    uint32_t value;
} VirtioInputEvent;

static ViMMIODevice _kbd_dev;
static ViQueue _eventq;

static bool _keyboard_ok = false;

static uint32_t _modifiers;
static bool _caps_lock;

static VirtioInputEvent* _events;
static VirtioInputEvent** _event_by_desc;
static uint16_t _posted;

static inline bool is_press_or_repeat(uint32_t value)
{
    return value == 1u || value == 2u;
}

static void update_modifiers(uint16_t code, uint32_t value)
{
    const bool pressed = is_press_or_repeat(value);

    switch (code)
    {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            if (pressed) _modifiers |= KMOD_SHIFT; else _modifiers &= ~KMOD_SHIFT;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            if (pressed) _modifiers |= KMOD_CTRL; else _modifiers &= ~KMOD_CTRL;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            if (pressed) _modifiers |= KMOD_ALT; else _modifiers &= ~KMOD_ALT;
            break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            if (pressed) _modifiers |= KMOD_META; else _modifiers &= ~KMOD_META;
            break;
        default:
            break;
    }

    if (code == KEY_CAPSLOCK && value == 1u)
    {
        _caps_lock = !_caps_lock;
    }
}

static char letter_for_keycode(uint16_t code)
{
    switch (code)
    {
        case KEY_A: return 'a';
        case KEY_B: return 'b';
        case KEY_C: return 'c';
        case KEY_D: return 'd';
        case KEY_E: return 'e';
        case KEY_F: return 'f';
        case KEY_G: return 'g';
        case KEY_H: return 'h';
        case KEY_I: return 'i';
        case KEY_J: return 'j';
        case KEY_K: return 'k';
        case KEY_L: return 'l';
        case KEY_M: return 'm';
        case KEY_N: return 'n';
        case KEY_O: return 'o';
        case KEY_P: return 'p';
        case KEY_Q: return 'q';
        case KEY_R: return 'r';
        case KEY_S: return 's';
        case KEY_T: return 't';
        case KEY_U: return 'u';
        case KEY_V: return 'v';
        case KEY_W: return 'w';
        case KEY_X: return 'x';
        case KEY_Y: return 'y';
        case KEY_Z: return 'z';
        default:
            return 0;
    }
}

static char map_key_to_ascii(uint16_t code)
{
    const bool shift = (_modifiers & KMOD_SHIFT) != 0;

    switch (code)
    {
        case KEY_ENTER: return '\n';
        case KEY_TAB: return '\t';
        case KEY_SPACE: return ' ';
        case KEY_BACKSPACE: return '\b';
        case KEY_ESC: return 0x1B;
        default: break;
    }

    // letters (Linux input keycodes are not contiguous)
    {
        char base = letter_for_keycode(code);
        if (base)
        {
            const bool upper = shift ^ _caps_lock;
            return upper ? (char)(base - 'a' + 'A') : base;
        }
    }

    // number row + punctuation
    switch (code)
    {
        case KEY_1: return shift ? '!' : '1';
        case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3';
        case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5';
        case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7';
        case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9';
        case KEY_0: return shift ? ')' : '0';
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFTBRACE: return shift ? '{' : '[';
        case KEY_RIGHTBRACE: return shift ? '}' : ']';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';
        default:
            return 0;
    }
}

static bool post_one_buffer(uint16_t slot)
{
    int head = virtq_alloc_chain(&_eventq, 1);
    if (head < 0)
    {
        return false;
    }

    uint16_t d0 = (uint16_t)head;

    _eventq.descriptor[d0].address = (uint64_t)(uintptr_t)&_events[slot];
    _eventq.descriptor[d0].length = (uint32_t)sizeof(VirtioInputEvent);
    _eventq.descriptor[d0].flags = (uint16_t)(_eventq.descriptor[d0].flags | VIRTQ_DESC_F_WRITE);

    _event_by_desc[d0] = &_events[slot];

    virtq_submit(&_eventq, d0);
    _posted++;

    return true;
}

bool virtio_keyboard_init(void)
{
    if (_keyboard_ok)
    {
        return true;
    }

    memory_init();

    if (!virtio_mmio_find_device(VIRTIO_DEVICE_ID_INPUT, &_kbd_dev))
    {
        return false;
    }

    printf("virtio-kbd: found @0x%x v%u\n", (unsigned)_kbd_dev.base, (unsigned)_kbd_dev.version);

    if (!virtio_mmio_init(&_kbd_dev))
    {
        printf("virtio-kbd: mmio init failed\n");
        return false;
    }

    uint64_t accepted = 0;
    if (!virtio_mmio_negotiate(&_kbd_dev, 0, &accepted))
    {
        printf("virtio-kbd: negotiate failed\n");
        return false;
    }

    // Queue 0 is the event queue.
    if (!virtq_init(&_kbd_dev, 0, 64, &_eventq))
    {
        printf("virtio-kbd: eventq init failed\n");
        return false;
    }

    // Allocate buffers and post them.
    const uint16_t qsz = _eventq.queue_size;

    _events = (VirtioInputEvent*)kmalloc_aligned(sizeof(VirtioInputEvent) * qsz, 8);
    _event_by_desc = (VirtioInputEvent**)kmalloc_aligned(sizeof(VirtioInputEvent*) * qsz, 4);

    if (!_events || !_event_by_desc)
    {
        printf("virtio-kbd: alloc failed\n");
        return false;
    }

    for (uint16_t i = 0; i < qsz; i++)
    {
        _events[i].type = 0;
        _events[i].code = 0;
        _events[i].value = 0;
        _event_by_desc[i] = 0;
    }

    _posted = 0;
    for (uint16_t i = 0; i < qsz; i++)
    {
        if (!post_one_buffer(i))
        {
            printf("virtio-kbd: failed to post buffers\n");
            return false;
        }
    }

    virtio_mmio_notify_queue(&_kbd_dev, 0);

    // DRIVER_OK
    uint32_t status = mmio_read32(_kbd_dev.base, VIRTIO_MMIO_STATUS);
    mmio_write32(_kbd_dev.base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_DRIVER_OK);
    fence_iorw();

    _modifiers = 0;
    _caps_lock = false;
    _keyboard_ok = true;

    printf("virtio-kbd: ready (buffers=%u)\n", (unsigned)_posted);
    return true;
}

bool virtio_keyboard_poll_event(KeyboardEvent* out_event)
{
    if (!_keyboard_ok || !out_event)
    {
        return false;
    }

    for (unsigned attempts = 0; attempts < 8; attempts++)
    {
        uint16_t used_id;
        if (!virtq_poll_used(&_eventq, &used_id))
        {
            return false;
        }

        VirtioInputEvent* event = (used_id < _eventq.queue_size) ? _event_by_desc[used_id] : 0;
        if (!event)
        {
            virtq_submit(&_eventq, used_id);
            virtio_mmio_notify_queue(&_kbd_dev, 0);
            continue;
        }

        const uint16_t type = event->type;
        const uint16_t code = event->code;
        const uint32_t value = event->value;

        virtq_submit(&_eventq, used_id);
        virtio_mmio_notify_queue(&_kbd_dev, 0);

        if (type == EV_SYN)
        {
            continue;
        }

        if (type != EV_KEY)
        {
            continue;
        }

        update_modifiers(code, value);

        out_event->type = type;
        out_event->code = code;
        out_event->value = (int32_t)value;
        out_event->modifiers = _modifiers;
        out_event->ascii = is_press_or_repeat(value) ? map_key_to_ascii(code) : 0;

        return true;
    }

    return false;
}
