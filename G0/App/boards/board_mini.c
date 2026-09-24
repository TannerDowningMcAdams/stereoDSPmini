#include "board.h"
#include "main.h"

// Scan order follows the ADC ranks in CubeMX: channels 8, 9, 10, 7, 11.
static const BoardPot pots[] = {
    { 0u, 0u },
    { 1u, 1u },
    { 2u, 2u },
    { 3u, 3u },
    { 4u, 4u },
};

// ON is the right footswitch and AUX the left (plan §3.3).
static const BoardSwitch switches[] = {
    { FTSW_R_GPIO_Port,    FTSW_R_Pin,    SWITCH_ROLE_ON,        false },
    { FTSW_L_GPIO_Port,    FTSW_L_Pin,    SWITCH_ROLE_AUX,       false },
    { MODE_SW_H_GPIO_Port, MODE_SW_H_Pin, SWITCH_ROLE_MODE_UP,   true },
    { MODE_SW_L_GPIO_Port, MODE_SW_L_Pin, SWITCH_ROLE_MODE_DOWN, true },
};

static const BoardPwmLed pwmLeds[] = {
    { TIM17, 1u, SWITCH_ROLE_AUX },     // LED_1_PWM, PB9
    { TIM16, 1u, SWITCH_ROLE_ON },      // LED_2_PWM, PD0
};

// Bit 0 is the leftmost LED. The silkscreen numbers them from the back of the
// board, so LED_6 is on the left.
static const BoardGpioLed gpioLeds[] = {
    { LED_6_GPIO_Port, LED_6_Pin },
    { LED_5_GPIO_Port, LED_5_Pin },
    { LED_4_GPIO_Port, LED_4_Pin },
    { LED_3_GPIO_Port, LED_3_Pin },
};

_Static_assert(sizeof(pots) / sizeof(pots[0]) <= BOARD_MAX_POTS, "too many pots");
_Static_assert(sizeof(switches) / sizeof(switches[0]) <= BOARD_MAX_SWITCHES, "too many switches");
_Static_assert(sizeof(pwmLeds) / sizeof(pwmLeds[0]) <= BOARD_MAX_PWM_LEDS, "too many PWM LEDs");
_Static_assert(sizeof(gpioLeds) / sizeof(gpioLeds[0]) <= BOARD_MAX_GPIO_LEDS, "too many GPIO LEDs");

const BoardConfig kBoard = {
    pots,     sizeof(pots) / sizeof(pots[0]),
    switches, sizeof(switches) / sizeof(switches[0]),
    pwmLeds,  sizeof(pwmLeds) / sizeof(pwmLeds[0]),
    gpioLeds, sizeof(gpioLeds) / sizeof(gpioLeds[0]),
};
