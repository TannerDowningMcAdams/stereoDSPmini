#pragma once

#include <stdbool.h>
#include <stdint.h>

// Resolves the footswitch pair into gestures (plan §3.2 rules 2-5). A chord never
// leaks a SHORT or a single HOLD. Mode switch steps pass through. Consumes the
// switch.c queue; no HAL.

typedef enum {
    GESTURE_ON_SHORT,
    GESTURE_ON_HOLD,
    GESTURE_AUX_SHORT,      // pressUs is the tap instant (rule 5)
    GESTURE_AUX_HOLD,
    GESTURE_BOTH_HOLD,
    GESTURE_MODE_UP,        // short press, hold, and each repeat
    GESTURE_MODE_DOWN
} GestureType;

typedef struct {
    GestureType type;
    uint32_t    pressUs;
} Gesture;

// Switch indices as passed to switch.c.
void gestureInit(uint8_t onIndex, uint8_t auxIndex, uint8_t upIndex, uint8_t downIndex);

// Call until it returns false, once per tick after switchPoll().
bool gestureNext(uint32_t nowUs, Gesture* gesture);
