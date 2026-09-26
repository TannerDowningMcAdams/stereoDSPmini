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

    // DSP runs in PendSV. Thread mode is left for work of any length, such as
    // engine activation.
    for(;;)
    {
        gSystem.poll();
        __NOP();
    }
}

// From PendSV_Handler, at priority 15.
extern "C" void App_PendSV(void)
{
    gSystem.audioPendSV();
}
