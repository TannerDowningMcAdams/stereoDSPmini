#include "bootloader.h"
#include "stm32g0xx.h"

#define BOOTLOADER_MAGIC   0xB0071DE5UL
#define SYSTEM_MEMORY_BASE 0x1FFF0000UL   // AN2606, STM32G03x/04x

static void backupAccessEnable(void)
{
    RCC->APBENR1 |= RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN;
    (void) RCC->APBENR1;   // Let the clock enable land before touching PWR/TAMP
    PWR->CR1 |= PWR_CR1_DBP;
}

// Returns PWR and TAMP to their reset state, which the system bootloader assumes.
static void backupAccessDisable(void)
{
    PWR->CR1 &= ~PWR_CR1_DBP;
    RCC->APBENR1 &= ~(RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN);
}

void bootloaderCheckAndJump(void)
{
    // Static, not local: at -O0 a local is read back from the stack after the MSP moves.
    static void (*systemBootloader)(void);

    // Power-on with blank flash sets PROGEMPTY; left set, every later reset would boot the bootloader.
    FLASH->ACR &= ~FLASH_ACR_PROGEMPTY;

    backupAccessEnable();
    uint32_t request = TAMP->BKP0R;
    TAMP->BKP0R = 0;   // Cleared first, so any later reset boots the app
    backupAccessDisable();

    if (request != BOOTLOADER_MAGIC) { return; }

    systemBootloader = (void (*)(void)) (*(volatile uint32_t *) (SYSTEM_MEMORY_BASE + 4u));
    __set_MSP(*(volatile uint32_t *) SYSTEM_MEMORY_BASE);
    systemBootloader();
}

void bootloaderRequest(void)
{
    __disable_irq();
    backupAccessEnable();
    TAMP->BKP0R = BOOTLOADER_MAGIC;
    NVIC_SystemReset();
}
