 #include "audio.hpp"
#include "g0_spi.hpp"
#include "spi_protocol.h"
#include "stm32h7xx_hal_spi.h"
#include "system.hpp"
#include "stm32h7xx_hal_def.h"
#include <cstdint>
#include <cstring>
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#ifdef __cplusplus
}
#endif

UNCACHED_RAM SpiControlPacket G0Spi::rxPacketDMA_;
UNCACHED_RAM SpiControlPacket G0Spi::txPacketDMA_;

extern System gSystem;

void G0Spi::init()
{
    HAL_StatusTypeDef spiTxRxStatus = HAL_SPI_TransmitReceive_DMA(handle_, reinterpret_cast<uint8_t*>(&txPacketDMA_), reinterpret_cast<uint8_t*>(&rxPacketDMA_), kSpiPacketWords);
    
    if (spiTxRxStatus != HAL_OK) 
    {
        status_ = CommStatus::ERROR;
        spiErrorHandler(); 
    }

    else { status_ = CommStatus::OK; }
}

void G0Spi::packUnpackSpiData()
{
    uint16_t messageId = rxPacketDMA_.messageId;

    switch (messageId)
    {
        //Control packet 
        case 0x5A9E: parseControlPacket();
        break;
    }

    // Populate tx packet if necessary

    HAL_SPI_TransmitReceive_DMA(handle_, reinterpret_cast<uint8_t*>(&txPacketDMA_), reinterpret_cast<uint8_t*>(&rxPacketDMA_), kSpiPacketWords);
}

void G0Spi::parseControlPacket()
{
    uint16_t flags = rxPacketDMA_.flags;

    // Mode switch is a 2-bit unsigned integer (3 values used)
    params_.modeSwitch = flags & MODE_SWITCH_MASK;

    // Boolean values for relay states
    params_.relayR = (flags & FLAG_RELAY_RIGHT) != 0;
    params_.relayL = (flags & FLAG_RELAY_LEFT) != 0;

    params_.killWet = (flags & FLAG_KILL_WET) != 0;

    // 12-bit mask for VCA value
    params_.vcaValue = rxPacketDMA_.vcaValue & (0x0FFF);

    // 5 potentiometer values converted from 12-bit unsigned int to float
    params_.potentiometers[0] = (rxPacketDMA_.pot[0] & (0x0FFF)) * G0Spi::int12ToFloat;
    params_.potentiometers[1] = (rxPacketDMA_.pot[1] & (0x0FFF)) * G0Spi::int12ToFloat;
    params_.potentiometers[2] = (rxPacketDMA_.pot[2] & (0x0FFF)) * G0Spi::int12ToFloat;
    params_.potentiometers[3] = (rxPacketDMA_.pot[3] & (0x0FFF)) * G0Spi::int12ToFloat;
    params_.potentiometers[4] = (rxPacketDMA_.pot[4] & (0x0FFF)) * G0Spi::int12ToFloat;

    params_.beatsPerSecond = rxPacketDMA_.beatsPerSecond;
    params_.clockPhase = rxPacketDMA_.clockPhase;

}

void G0Spi::spiErrorHandler(){
    return;
}




