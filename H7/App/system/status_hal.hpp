#pragma once
#include "status.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

// For the hardware-facing classes only; nothing above them should need it.
Status fromHAL(HAL_StatusTypeDef halStatus);
