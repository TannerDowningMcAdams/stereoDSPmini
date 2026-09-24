#pragma once

#include "stm32g0xx.h"
#include <stdbool.h>
#include <stdint.h>

// The interface every board file implements. A board is const tables only; the core
// iterates them, so a new pedal is one board_<name>.c selected by -DBOARD=<name>.

#define BOARD_MAX_POTS      8u
#define BOARD_MAX_SWITCHES  6u
#define BOARD_MAX_PWM_LEDS  2u
#define BOARD_MAX_GPIO_LEDS 4u

typedef enum {
    SWITCH_ROLE_ON,
    SWITCH_ROLE_AUX,
    SWITCH_ROLE_MODE_UP,
    SWITCH_ROLE_MODE_DOWN
} SwitchRole;

typedef struct {
    uint8_t adcIndex;       // position in the ADC scan, which CubeMX configures
    uint8_t paramIndex;     // slot in the outgoing param[]
} BoardPot;

// Pulled up, closed to ground.
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
    SwitchRole    role;
    bool          repeat;   // HOLD is followed by REPEAT while held
} BoardSwitch;

typedef struct {
    TIM_TypeDef* tim;
    uint8_t      channel;   // 1..4
    SwitchRole   role;      // the footswitch it sits above
} BoardPwmLed;

typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} BoardGpioLed;

typedef struct {
    const BoardPot*     pots;      uint8_t numPots;
    const BoardSwitch*  switches;  uint8_t numSwitches;
    const BoardPwmLed*  pwmLeds;   uint8_t numPwmLeds;
    const BoardGpioLed* gpioLeds;  uint8_t numGpioLeds;    // [0] is bit 0
} BoardConfig;

// Defined in exactly one boards/board_*.c.
extern const BoardConfig kBoard;

#define BOARD_NONE 0xFFu

// Index of the first switch with this role, or BOARD_NONE.
static inline uint8_t boardSwitchIndex(SwitchRole role)
{
    for (uint8_t i = 0; i < kBoard.numSwitches; i++)
    {
        if (kBoard.switches[i].role == role) { return i; }
    }
    return BOARD_NONE;
}

// Index of the PWM LED above the switch with this role, or BOARD_NONE.
static inline uint8_t boardPwmLedIndex(SwitchRole role)
{
    for (uint8_t i = 0; i < kBoard.numPwmLeds; i++)
    {
        if (kBoard.pwmLeds[i].role == role) { return i; }
    }
    return BOARD_NONE;
}
