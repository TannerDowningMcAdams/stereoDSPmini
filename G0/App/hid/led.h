#pragma once

#include <stdbool.h>
#include <stdint.h>

// The board's LEDs, rendered on the 1 ms tick. PWM LEDs play patterns through a
// gamma curve and a dimmer; GPIO LEDs are plain on/off.

typedef enum {
    LED_OFF,
    LED_ON,
    LED_BLINK,      // periodMs per on/off cycle
    LED_RAMP,       // from the current brightness to level over periodMs, then hold
    LED_PULSE,      // triangle 0 -> level -> 0 over periodMs
    LED_BLINK_N     // count blinks of periodMs each, then off
} LedMode;

typedef struct {
    LedMode  mode;
    uint8_t  level;
    uint16_t periodMs;
    uint8_t  count;
} LedPattern;

void ledInit(void);

// Restarts the pattern only when it differs from the one playing, so the caller can
// set the wanted pattern on every tick.
void ledPwmSet(uint8_t led, const LedPattern* pattern);
// A BLINK_N pattern has finished its blinks.
bool ledPwmDone(uint8_t led);

// Scales every PWM LED after gamma. 255 is full brightness.
void ledSetDimmer(uint8_t scale);

// Bit i lights GPIO LED i.
void ledGpioSet(uint8_t mask);

void ledRender(void);
