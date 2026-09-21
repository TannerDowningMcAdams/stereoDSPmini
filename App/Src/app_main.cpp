#include "system.hpp"
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

// Single global system instance
System gSystem;

extern "C" void App_Run(void)
{
    // Initialize peripheral objects and interrupts
    gSystem.init();

    // All DSP runs here, below every interrupt. No WFI: a block published between
    // poll() and WFI would sleep until SysTick (1 ms), past the block deadline.
    for(;;)
    {
        gSystem.poll();
        __NOP();
    }
}
