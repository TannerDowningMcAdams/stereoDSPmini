#include "audio.hpp"
#include "g0_spi.hpp"
#include "system.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include <cstdint>
#include <cstring>

UNCACHED_RAM uint16_t spiRxDataDMA_[G0Spi::kSpiPacketWords];
UNCACHED_RAM uint16_t spiTxDataDMA_[G0Spi::kSpiPacketWords];

extern System gSystem;

void G0Spi::init(){

    HAL_StatusTypeDef spiTxRxStatus = HAL_SPI_TransmitReceive_DMA(handle_, (uint8_t *)&spiTxDataDMA_, (uint8_t *)&spiRxDataDMA_, kSpiPacketWords);

    if (spiTxRxStatus != HAL_OK) {

        status_ = CommStatus::ERROR;
        spiErrorHandler(); 

    }

    else { status_ = CommStatus::OK; }

}

void G0Spi::packUnpackSpiData() {

    std::memcpy(spiRxDataCache_, const_cast<uint16_t*>(spiRxDataDMA_), sizeof(spiRxDataCache_));
    uint16_t messageId = spiRxDataCache_[0];

    switch (messageId) {

        //Control packet 
        case 0x5A9E: parseControlPacket();
        break;

    }

    std::memcpy(const_cast<uint16_t*>(spiTxDataDMA_), spiTxDataCache_, sizeof(spiTxDataCache_));
    HAL_SPI_TransmitReceive_DMA(handle_, (uint8_t *)&spiTxDataDMA_, (uint8_t *)&spiRxDataDMA_, kSpiPacketWords);

}

void G0Spi::parseControlPacket() {

    // Boolean values for relay states
    params_.relayL = (spiRxDataCache_[0] & (1)) != 0;
    params_.relayR = (spiRxDataCache_[0] & (1 << 1)) != 0;

    // Mode switch is a 2-bit unsigned integer (3 values used)
    params_.modeSwitch = (spiRxDataCache_[0] >> 2) & (0b11);

    // 12-bit mask for VCA value
    params_.vcaValue = spiRxDataCache_[2] & (0x0FFF);

    // 5 potentiometer values converted from 12-bit unsigned int to float
    params_.potentiometers[0] = (spiRxDataCache_[3] & (0x0FFF)) * G0Spi::int12_to_float;
    params_.potentiometers[1] = (spiRxDataCache_[4] & (0x0FFF)) * G0Spi::int12_to_float;
    params_.potentiometers[2] = (spiRxDataCache_[5] & (0x0FFF)) * G0Spi::int12_to_float;
    params_.potentiometers[3] = (spiRxDataCache_[6] & (0x0FFF)) * G0Spi::int12_to_float;
    params_.potentiometers[4] = (spiRxDataCache_[7] & (0x0FFF)) * G0Spi::int12_to_float;

    params_.beatsPerSecond = spiRxDataCache_[8];
    params_.clockPhase = spiRxDataCache_[9];

}





