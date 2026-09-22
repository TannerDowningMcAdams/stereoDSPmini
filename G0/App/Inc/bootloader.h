#pragma once

// Call first in main(), before HAL_Init(). Returns only if no bootloader request is pending.
void bootloaderCheckAndJump(void);

// Flags a request in a TAMP backup register and resets. Does not return.
void bootloaderRequest(void);
