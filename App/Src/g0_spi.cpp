#include "audio.hpp"
#include "g0_spi.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include <cstdint>
#include <cstring>

UNCACHED_RAM uint16_t spiRxDataDMA_[G0Spi::kSpiPacketWords];
UNCACHED_RAM uint16_t spiTxDataDMA_[G0Spi::kSpiPacketWords];


void G0Spi::init(){

    HAL_StatusTypeDef spiTxRxStatus = HAL_SPI_TransmitReceive_DMA(handle_, (uint8_t *)&spiTxDataDMA, (uint8_t *)&spiRxDataDMA, kSpiPacketWords);

    if (spiTxRxStatus != HAL_OK) {

        status_ = CommStatus::ERROR;
        spiErrorHandler(); 

    }

    else { status_ = CommStatus::OK; }

}



void G0Spi::packUnpackSpiData() {

    std::memcpy(spiRxDataCache_, const_cast<uint16_t*>(spiRxDataDMA_), sizeof(spiRxDataCache_));

    uint16_t message_id = spiRxDataCache_[0];

    switch (message_id) {

        //Control packet 
        case 0x5A9E: parseControlPacket();
        break;

    }

    std::memcpy(const_cast<uint16_t*>(spiTxDataDMA_), spiTxDataCache_, sizeof(spiTxDataCache_));

    HAL_SPI_TransmitReceive_DMA(handle_, (uint8_t *)&spiTxDataDMA_, (uint8_t *)&spiRxDataDMA_, kSpiPacketWords);

}

void G0Spi::parseControlPacket() {

    uint16_t bit_mask = word_1 & (1 << 15);

    //12-bit mask for VCA value
    uint16_t vca_value = spiRxDataCache_[2] & (0x0FFF);
    uint16_t ctrl_1 = spiRxDataCache_[3] & (0x0FFF);
    uint16_t ctrl_2 = spiRxDataCache_[4] & (0x0FFF);
    uint16_t ctrl_3 = spiRxDataCache_[5] & (0x0FFF);
    uint16_t ctrl_4 = spiRxDataCache_[6] & (0x0FFF);
    uint16_t ctrl_5 = spiRxDataCache_[7] & (0x0FFF);

    float tempo = spiRxDataCache_[8];
    uint16_t clock_phase = spiRxDataCache_[9]
}

