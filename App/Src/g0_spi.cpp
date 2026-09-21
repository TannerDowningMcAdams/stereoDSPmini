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
    config_ = config;

    // Armed at an arbitrary point in the G0's cadence. Landing mid-burst fails the
    // CRC, and the resync path realigns it.
    if (arm() == Status::OK) { status_ = Status::OK; }
    else
    {
        status_ = Status::ERROR;
        scheduleResync();
    }
}

bool G0Spi::txRxComplete()
{
    // Parse before re-arming: the next transfer lands in the same buffer.
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

    // Re-armed at once, which lands in the idle gap after this packet. A failure
    // raises no callback, so it goes through the same resync path as an error.
    if (arm() != Status::OK) { scheduleResync(); }
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

// Tick before flag, so serviceErrors() never pairs the flag with an older tick.
void G0Spi::scheduleResync()
{
    errorTick_     = HAL_GetTick();
    resyncPending_ = 1u;
}

// Every HAL error path (CRC, overrun, mode fault, DMA) has already stopped the
// transfer and left the handle READY by the time this runs.
void G0Spi::spiErrorHandler()
{
    errors_     = errors_ | HAL_SPI_GetError(config_.spi);
    errorCount_ = errorCount_ + 1u;
    scheduleResync();
}

void G0Spi::serviceErrors()
{
    if (resyncPending_ == 0u) { return; }
    // Software NSS gives the slave no framing, so re-arming mid-burst would shift
    // every packet after it. Wait until the faulted burst is over.
    if ((HAL_GetTick() - errorTick_) < kResyncDelayMs) { return; }
    resyncPending_ = 0u;

    (void) HAL_SPI_Abort(config_.spi);
    if (arm() == Status::OK) { status_ = Status::OK; }
    else
    {
        status_ = Status::ERROR;
        scheduleResync();
    }
}
