#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

// Enum class mirrors HAL Status Typedef
enum class Status : uint8_t
{
    OK      = 0,
    BUSY    = 1,
    ERROR   = 2,
    TIMEOUT = 3,
    INIT    = 4   // Not yet initialized
};

// Inline conversion matches 
inline Status fromHAL(HAL_StatusTypeDef halStatus)
{
    return static_cast<Status>(halStatus);
}