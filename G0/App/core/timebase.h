#pragma once

#include <stdbool.h>
#include <stdint.h>

// Free-running time from a hardware timer. Unlike SysTick counts, it keeps time
// through a flash erase stall. Both clocks wrap cleanly at 2^32; compare them
// only by difference.

void timebaseInit(void);

// Callable from thread mode or any ISR.
uint32_t timebaseNowUs(void);
uint32_t timebaseNowMs(void);

// A deadline is a timebaseNowMs() value. Valid for spans below 2^31 ms.
uint32_t deadlineSet(uint32_t fromNowMs);
bool     deadlineExpired(uint32_t deadline);
