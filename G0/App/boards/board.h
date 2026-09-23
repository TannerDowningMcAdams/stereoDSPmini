#pragma once

#include "stm32g0xx.h"
#include <stdint.h>

// The interface every board file implements. A board is const tables only; the core
// iterates them, so a new pedal is one board_<name>.c selected by -DBOARD=<name>.

#define BOARD_MAX_POTS      8u
#define BOARD_MAX_SWITCHES  6u
#define BOARD_MAX_PWM_LEDS  2u
#define BOARD_MAX_GPIO_LEDS 4u
#define BOARD_MAX_ENGINES   16u

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
} BoardSwitch;

typedef struct {
    TIM_TypeDef* tim;
    uint8_t      channel;   // 1..4
} BoardPwmLed;

typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} BoardGpioLed;

typedef struct {
    const BoardPot*     pots;      uint8_t numPots;
    const BoardSwitch*  switches;  uint8_t numSwitches;
    const BoardPwmLed*  pwmLeds;   uint8_t numPwmLeds;     // [0] above AUX, [1] above ON
    const BoardGpioLed* gpioLeds;  uint8_t numGpioLeds;    // [0] is bit 0
    const uint8_t*      engines;   uint8_t numEngines;     // ENGINE_* ids, in selection order
} BoardConfig;

// Defined in exactly one boards/board_*.c.
extern const BoardConfig kBoard;
