#include "g0_spi.hpp"
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#ifdef __cplusplus
}
#endif

UNCACHED_RAM SpiControlPacket G0Spi::rxPacketDMA_;
UNCACHED_RAM SpiControlPacket G0Spi::txPacketDMA_;

void G0Spi::init(const Config& config)
{
    // Not armed here: the first CS rising edge does it, on a packet boundary.
    config_ = config;
}

bool G0Spi::txRxComplete()
{
    // Parsed before onFrameEnd() re-arms into this buffer. Both run at priority 2,
    // so that cannot preempt this.
    bool controlUpdated = false;
    switch (rxPacketDMA_.messageId)
    {
        case kControlPacketId:
            parseControlPacket();
            controlUpdated = true;
            break;
        default:
            break;
    }

    // Populate tx packet if necessary

    // Not re-armed here; onFrameEnd() does it once the G0 raises CS.
    return controlUpdated;
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

Status G0Spi::arm()
{
    return fromHAL(HAL_SPI_TransmitReceive_DMA(config_.spi,
                                               reinterpret_cast<uint8_t*>(&txPacketDMA_),
                                               reinterpret_cast<uint8_t*>(&rxPacketDMA_),
                                               kSpiPacketWords));
}

// Every HAL error path (CRC, overrun, mode fault, DMA) has already stopped the
// transfer and left the handle READY by the time this runs.
void G0Spi::spiErrorHandler()
{
    errors_     = errors_ | HAL_SPI_GetError(config_.spi);
    errorCount_ = errorCount_ + 1u;
}

void G0Spi::onFrameEnd()
{
    // EXTI is live from MX_GPIO_Init, before init() runs. Nothing is armed until
    // config_ is set.
    if (config_.spi == nullptr) { return; }
    frameCount_ = frameCount_ + 1u;

    // Still busy when the G0 ends its frame means the transfer began mid-packet.
    // Abort is quick here: its suspend-wait only runs in master mode.
    if (HAL_SPI_GetState(config_.spi) != HAL_SPI_STATE_READY)
    {
        resyncCount_ = resyncCount_ + 1u;
        (void) HAL_SPI_Abort(config_.spi);
    }

    // A failed arm needs no retry logic: the next frame end tries again.
    status_ = arm();
}
