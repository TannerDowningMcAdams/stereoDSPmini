#pragma once

#include <stdbool.h>
#include <stdint.h>

// Debounce and timing for momentary switches (plan §3.2 rules 1, 2, 6, 7). No HAL:
// the caller samples the pins and passes the time, so this builds on the host.

#define SWITCH_MAX         6u
#define SWITCH_HOLD_US     1000000u
#define SWITCH_REPEAT_US   250000u

typedef enum {
    SWITCH_EVENT_PRESS,
    SWITCH_EVENT_SHORT,     // released before the hold threshold; sent before its RELEASE
    SWITCH_EVENT_HOLD,
    SWITCH_EVENT_REPEAT,    // after HOLD, on switches in repeatMask
    SWITCH_EVENT_RELEASE
} SwitchEventType;

typedef struct {
    uint8_t         index;
    SwitchEventType type;
    uint32_t        pressUs;    // debounced press edge of this press
} SwitchEvent;

// downMask bit i is set while switch i is closed. A switch closed now is ignored
// until it has been released (rule 7).
void switchInit(uint8_t count, uint32_t repeatMask, uint32_t downMask);

// Once per 1 ms tick.
void switchPoll(uint32_t downMask, uint32_t nowUs);

bool switchNextEvent(SwitchEvent* event);
