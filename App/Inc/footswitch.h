#pragma once

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {

    FS_TYPE_BYPASS,
    FS_TYPE_TAP,
    FS_TYPE_PARAMETER

} FootswitchType;

typedef enum {

    FS_IDLE,
    FS_DEBOUNCE,
    FS_PRESSED,
    FS_HELD,              // threshold crossed, waiting for release
    FS_TAP_PENDING        // released, waiting to see if another tap follows

} FootswitchState;

typedef struct {

    FootswitchType type;
    FootswitchState state;
    uint16_t gpioPin;
    GPIO_TypeDef * gpioPort;
    bool physical;
    uint32_t timePressed;
    uint32_t timeReleased;

} Footswitch;

void FootswitchInit(FootswitchState state, uint16_t pin, GPIO_TypeDef* port);
void updateFootswitchTap(Footswitch* fs);
void updateFootswitchBypass(Footswitch* fs);
