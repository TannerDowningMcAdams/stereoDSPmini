#include "app.h"
#include "adc.h"
#include "iwdg.h"
#include "main.h"
#include <stdint.h>
#include "spi.h"
#include "spi_protocol.h"
#include "tim.h"

#define NUM_POTENTIOMETERS 5

// TODO: move into Protocol with the v2 packet. Must match the H7's kControlPacketId.
#define CONTROL_PACKET_ID 0x5A9E

// TIM1 10 ms frame: ADC scan at 0 ms (done ~6.9 ms, then pack), SPI frame at 8 ms (~70 us).
// This ordering keeps packing clear of the SPI DMA.

static volatile uint16_t potAdcBufferDMA[NUM_POTENTIOMETERS];
static uint16_t potValues[NUM_POTENTIOMETERS];
// The packet struct is packed (alignment 1); the SPI DMA moves halfwords.
static SpiControlPacket spiTxPacket __attribute__((aligned(4)));
static SpiControlPacket spiRxPacket __attribute__((aligned(4)));
static volatile uint32_t spiFrameCount = 0;

static void packSpiData(void);
static void startSpiFrame(void);
static void endSpiFrame(void);

void appInit(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) potAdcBufferDMA, NUM_POTENTIOMETERS);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
}

void appPoll(void)
{
    // Refresh only while SPI frames are completing, so a stalled timer, DMA or SPI
    // resets the G0 rather than leaving the H7 holding stale controls.
    static uint32_t lastFrameCount = 0;
    uint32_t frameCount = spiFrameCount;
    if (frameCount != lastFrameCount)
    {
        lastFrameCount = frameCount;
        HAL_IWDG_Refresh(&hiwdg);
    }
}

// ADC DMA complete, ADC1 priority 1
void onAdcReady(void)
{
    for (uint32_t i = 0; i < NUM_POTENTIOMETERS; i++)
    {
        potValues[i] = potAdcBufferDMA[i];
    }
    packSpiData();
}

static void packSpiData(void)
{
    spiTxPacket.messageId = CONTROL_PACKET_ID;
    for (uint32_t i = 0; i < NUM_POTENTIOMETERS; i++)
    {
        spiTxPacket.pot[i] = potValues[i];
    }
}

static void startSpiFrame(void)
{
    // A frame still in flight means the previous one hung; leave it for the watchdog.
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) { return; }

    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t *) &spiTxPacket,
                                    (uint8_t *) &spiRxPacket, SPI_PACKET_NUM_WORDS) != HAL_OK)
    {
        // Abandon the frame. The H7 re-arms on the rising edge regardless.
        HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
    }
}

// The rising edge is the H7's end-of-packet signal, so raise CS on every outcome.
static void endSpiFrame(void)
{
    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
    spiFrameCount = spiFrameCount + 1u;
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) { startSpiFrame(); }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) { endSpiFrame(); }
}

// Includes a CRC mismatch on what the H7 sent back, e.g. when it is not yet armed.
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) { endSpiFrame(); }
}
