#pragma once

#include <stdbool.h>

// Why the G0 started (plan §3.5, §4.3). The only reader of the RCC reset flags,
// which are cleared on read.

// Call first in appInit(). On a cold boot with AUX held, enters the system
// bootloader and does not return.
void bootStateInit(void);

// Power-on or brown-out. False after an IWDG or software reset.
bool bootWasCold(void);
