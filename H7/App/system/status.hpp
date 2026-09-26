#pragma once
#include <cstdint>

// Shared result type. It is included everywhere, so it stays free of the HAL.
// The HAL mapping lives in status_hal.hpp.
enum class Status : uint8_t
{
    OK      = 0,
    ERROR   = 1,
    BUSY    = 2,
    TIMEOUT = 3,
    INIT    = 4   // Not yet initialized
};
