#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

// A GPIO port and pin that always travel together. Built from CubeMX's
// *_GPIO_Port / *_Pin macro pairs, so the .ioc stays the source of truth.
struct Pin
{
    GPIO_TypeDef* port;
    uint16_t      mask;     // GPIO_PIN_x, exactly as CubeMX's *_Pin macros define it
};
