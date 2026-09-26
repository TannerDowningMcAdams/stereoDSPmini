#pragma once
#include <cstdint>

// Shared result type. Standalone on purpose: it is included everywhere, so it
// must not drag the HAL in. The HAL mapping lives in status_hal.hpp.
enum class Status : uint8_t
{
    OK      = 0,
    ERROR   = 1,
    BUSY    = 2,
    TIMEOUT = 3,
    INIT    = 4   // Not yet initialized
};
