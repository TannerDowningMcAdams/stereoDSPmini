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

    for(;;)
    {
        __WFI();
    }
}
