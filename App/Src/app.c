#include "app.h"
#include "adc.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "spi.h"
#include "spi_protocol.h"
#include "stm32g0xx_hal_def.h"
#include "stm32g0xx_hal_tim.h"
#include "tim.h"

#define NUM_POTENTIOMETERS 5

static volatile bool adcDataReadyFlag = false;
static volatile uint16_t potAdcBufferDMA[NUM_POTENTIOMETERS];
uint16_t potValues[NUM_POTENTIOMETERS];
static volatile uint16_t spiTxBuffer[SPI_PACKET_NUM_WORDS];
static volatile uint16_t spiRxBuffer[SPI_PACKET_NUM_WORDS];

bool adcDataReady() { return adcDataReadyFlag; }

void appInit()
{
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) potAdcBufferDMA, NUM_POTENTIOMETERS);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);   
}

void onAdcReady()   
{ 
    // Snapshot of potentiometer values from DMA buffer
    memcpy((void*)&potAdcBufferDMA, &potValues, NUM_POTENTIOMETERS * sizeof(uint16_t));
    adcDataReadyFlag = 1; 
}

void packSpiData()
{
    
    SpiControlPacket outgoingPacket;
    outgoingPacket.pot[0] = potValues[0];
    outgoingPacket.pot[1] = potValues[1];
    outgoingPacket.pot[2] = potValues[2];
    outgoingPacket.pot[3] = potValues[3];
    outgoingPacket.pot[4] = potValues[4];

}

void initiateSpiDma()
{
    HAL_StatusTypeDef SPI_status = HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*)&spiTxBuffer, (uint8_t*)&spiRxBuffer, SPI_PACKET_NUM_WORDS);
}
