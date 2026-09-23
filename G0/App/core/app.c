#include "app.h"
#include "adc.h"
#include "board.h"
#include "bootloader.h"
#include "engine_manifest.h"
#include "main.h"
#include "spi_link.h"
#include "tim.h"
#include "timebase.h"
#include <stdbool.h>
#include <stdint.h>

// TIM1 runs at 1 ms. CC1 starts each SPI frame (~100 us) and then wakes the thread
// tick, which builds the next frame. The ADC scan (~6.9 ms) is triggered by TRGO2 on
// the update event, which the repetition counter holds to every 10th period.

// Written by the ADC DMA; each halfword is one whole oversampled result.
static volatile uint16_t potAdcBufferDMA[BOARD_MAX_POTS];
static volatile uint32_t tickCount = 0;

static uint8_t  engine = ENGINE_PASSTHROUGH;
static uint16_t params[SPI_PARAM_COUNT];
static uint16_t discrete;
static uint16_t potOwnerMask;

static void appTick(void);

static bool switchHeld(SwitchRole role)
{
    for (uint8_t i = 0; i < kBoard.numSwitches; i++)
    {
        const BoardSwitch* sw = &kBoard.switches[i];
        if (sw->role == role) { return HAL_GPIO_ReadPin(sw->port, sw->pin) == GPIO_PIN_RESET; }
    }
    return false;
}

// True only for power-on or brown-out. PINRSTF is no use here: every internal reset
// also drives NRST. Flags are cleared so the next reset reports only its own cause.
static bool resetWasPowerOn(void)
{
    bool powerOn = __HAL_RCC_GET_FLAG(RCC_FLAG_PWRRST) != 0u;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return powerOn;
}

void appInit(void)
{
    // Cold boot only: a watchdog reset while AUX happens to be held must not strand
    // the G0 in its bootloader, since the H7 only probes for it at its own boot.
    const bool coldBoot = resetWasPowerOn();

    // Bench trigger: AUX held through power-on enters the system bootloader.
    if (coldBoot && switchHeld(SWITCH_ROLE_AUX))
    {
        HAL_Delay(20);
        if (switchHeld(SWITCH_ROLE_AUX)) { bootloaderRequest(); }
    }

    const EngineManifest* manifest = &kEngineManifest[engine];
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { params[i] = manifest->paramDefault[i]; }
    discrete = engineDiscreteDefault(manifest);
    for (uint8_t i = 0; i < kBoard.numPots; i++)
    {
        potOwnerMask = (uint16_t) (potOwnerMask | (1u << kBoard.pots[i].paramIndex));
    }

    timebaseInit();
    HAL_ADCEx_Calibration_Start(&hadc1);
    // numPots must equal the number of ranks CubeMX configures.
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) potAdcBufferDMA, kBoard.numPots);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
}

void appPoll(void)
{
    // Ticks missed during a stall collapse into one: every stage works from the
    // latest state, so there is nothing to catch up on.
    static uint32_t lastTick = 0;
    const uint32_t tick = tickCount;
    if (tick != lastTick)
    {
        lastTick = tick;
        appTick();
    }
}

static void readPots(void)
{
    for (uint8_t i = 0; i < kBoard.numPots; i++)
    {
        // 12-bit to the full 16-bit range, so full scale reads 65535.
        const uint16_t raw = potAdcBufferDMA[kBoard.pots[i].adcIndex];
        params[kBoard.pots[i].paramIndex] = (uint16_t) ((raw << 4) | (raw >> 8));
    }
}

static void buildFrame(void)
{
    G0ToH7Packet* tx = spiLinkBeginTx();
    if (tx == NULL) { return; }

    tx->runFlags    = 0u;
    tx->engine      = engine;
    tx->presetIndex = 0u;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { tx->param[i] = params[i]; }
    tx->discrete     = discrete;
    tx->eventToggles = 0u;
    tx->tempoHz      = 0.0f;
    tx->tempoPhase   = 0u;
    tx->ownerMask    = potOwnerMask;
    spiLinkCommitTx();
}

static void appTick(void)
{
    spiLinkPoll();
    readPots();
    buildFrame();
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        spiLinkStartFrame();
        tickCount = tickCount + 1u;
    }
}
