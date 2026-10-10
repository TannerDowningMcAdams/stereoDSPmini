#include "boot_state.h"
#include "board.h"
#include "bootloader.h"
#include "main.h"

static bool coldBoot;
static bool settingsWanted;

static bool switchHeld(SwitchRole role)
{
    const uint8_t index = boardSwitchIndex(role);
    if (index == BOARD_NONE) { return false; }
    return HAL_GPIO_ReadPin(kBoard.switches[index].port, kBoard.switches[index].pin) == GPIO_PIN_RESET;
}

// Read, wait out any bounce, read again.
static bool switchHeldSteady(SwitchRole role)
{
    if (!switchHeld(role)) { return false; }
    HAL_Delay(20);
    return switchHeld(role);
}

void bootStateInit(void)
{
    // PWRRSTF is set only by power-on or brown-out. PINRSTF is no use: every internal
    // reset also drives NRST. Cleared so the next reset reports only its own cause.
    coldBoot = __HAL_RCC_GET_FLAG(RCC_FLAG_PWRRST) != 0u;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    // Both checks are cold boot only. A watchdog reset while AUX happens to be held must
    // not strand the G0 in its bootloader, since the H7 only probes for it at its own
    // boot, and a player standing on ON when it fires must not land in a menu.
    if (!coldBoot) { return; }
    if (switchHeldSteady(SWITCH_ROLE_AUX)) { bootloaderRequest(); }
    settingsWanted = switchHeldSteady(SWITCH_ROLE_ON);
}

bool bootWasCold(void)
{
    return coldBoot;
}

bool bootWantsSettings(void)
{
    return settingsWanted;
}
