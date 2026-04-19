#ifndef STRATUS_VIRTIO_INPUT_H
#define STRATUS_VIRTIO_INPUT_H

// Minimal virtio-input (keyboard) support.
// Exposes Linux input-event style codes plus a convenience ASCII mapping.

#include <stdbool.h>

#include "platform.h"

bool virtio_keyboard_init(void);
bool virtio_keyboard_poll_event(KeyboardEvent* out_event);

#endif
