#include "app.h"
#include "adc.h"
#include "board.h"
#include "boot_state.h"
#include "gesture.h"
#include "led.h"
#include "main.h"
#include "pot.h"
#include "spi_link.h"
#include "switch.h"
#include "tim.h"
#include "timebase.h"
#include "ui_context.h"
#include <stdbool.h>
#include <stdint.h>

// TIM1 runs at 1 ms. CC1 starts each SPI frame (~100 us) and then wakes the thread
// tick, which runs input -> ui -> output. The ADC scan (under 1 ms) is triggered by
// TRGO2 on the update event, which the repetition counter holds to every 10th period.

_Static_assert(BOARD_MAX_SWITCHES <= SWITCH_MAX, "switch.c cannot hold every board switch");
_Static_assert(BOARD_MAX_POTS <= POT_MAX, "pot.c cannot hold every board pot");

// Written by the ADC DMA; each halfword is one whole oversampled result.
static volatile uint16_t potAdcBufferDMA[BOARD_MAX_POTS];
static volatile uint32_t adcScanCount = 0;
static volatile uint32_t tickCount = 0;

static void appTick(void);

static uint32_t readSwitches(void)
{
    uint32_t down = 0u;
    for (uint8_t i = 0; i < kBoard.numSwitches; i++)
    {
        const BoardSwitch* sw = &kBoard.switches[i];
        if (HAL_GPIO_ReadPin(sw->port, sw->pin) == GPIO_PIN_RESET) { down |= 1u << i; }
    }
    return down;
}

void appInit(void)
{
    bootStateInit();

    uint32_t repeatMask = 0u;
    for (uint8_t i = 0; i < kBoard.numSwitches; i++)
    {
        if (kBoard.switches[i].repeat) { repeatMask |= 1u << i; }
    }
    switchInit(kBoard.numSwitches, repeatMask, readSwitches());
    gestureInit(boardSwitchIndex(SWITCH_ROLE_ON), boardSwitchIndex(SWITCH_ROLE_AUX),
                boardSwitchIndex(SWITCH_ROLE_MODE_UP), boardSwitchIndex(SWITCH_ROLE_MODE_DOWN));
    potInit(kBoard.numPots);

    timebaseInit();
    ledInit();
    uiInit();

    HAL_ADCEx_Calibration_Start(&hadc1);
    // numPots must equal the number of ranks CubeMX configures.
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) potAdcBufferDMA, kBoard.numPots);
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
    static uint32_t lastScan = 0;
    const uint32_t scan = adcScanCount;
    if (scan == lastScan) { return; }
    lastScan = scan;
    for (uint8_t i = 0; i < kBoard.numPots; i++) { potUpdate(i, potAdcBufferDMA[kBoard.pots[i].adcIndex]); }
}

static void buildFrame(void)
{
    G0ToH7Packet* tx = spiLinkBeginTx();
    if (tx != NULL && uiFillFrame(tx)) { spiLinkCommitTx(); }
}

static void appTick(void)
{
    spiLinkPoll();
    switchPoll(readSwitches(), timebaseNowUs());
    readPots();
    uiTick();
    ledRender();
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

// ADC DMA complete: one scan of every pot has landed.
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) { adcScanCount = adcScanCount + 1u; }
}
