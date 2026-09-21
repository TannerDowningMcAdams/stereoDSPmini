#pragma once
#include "ui_params.hpp"
#include "status_hal.hpp"
#ifdef __cplusplus
extern "C" {
#endif
#include "spi_protocol.h"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class G0Spi {
public:

    struct Config
    {
        // Slave with software NSS: the G0 clocks each packet and there is no
        // hardware framing, which is what shapes serviceErrors().
        SPI_HandleTypeDef* spi;
    };

    G0Spi() = default;
    ~G0Spi() = default;

    static constexpr uint16_t kSpiPacketWords = SPI_PACKET_NUM_WORDS;

    void init(const Config& config);

    // ISR: true only when a valid control packet updated params(). Anything else
    // leaves the last good set, or the defaults, in force.
    bool txRxComplete();
    // ISR: latch the error and schedule a resync. Nothing is re-armed here.
    void spiErrorHandler();
    // Thread mode: re-arm a faulted transfer once the bus is between packets.
    void serviceErrors();

    const uiParams& params() const { return params_; }
    Status   status()     const { return status_; }
    uint32_t errorCount() const { return errorCount_; }
    // Accumulated HAL_SPI_ERROR_* bits since boot.
    uint32_t errors()     const { return errors_; }
    
private:

    Config   config_ {};
    uiParams params_ {};
    Status   status_ = Status::INIT;

    static constexpr uint16_t kControlPacketId = 0x5A9E;
    static constexpr float    int12ToFloat     = 1.0f / 4095.0f;
    // Packets arrive every 10 ms and last well under 1 ms. Three ticks is 2-3 ms:
    // past the end of any faulted burst, well short of the next one.
    static constexpr uint32_t kResyncDelayMs   = 3;

    // DMA buffers for SPI transmission and reception. Not volatile: the carve-out
    // is uncached, and volatile only invites a cast that strips it (UB).
    static SpiControlPacket rxPacketDMA_;
    static SpiControlPacket txPacketDMA_;

    // Every ISR writer runs at priority 2 (SPI2 and both DMA streams), so they
    // cannot preempt each other and need no critical section.
    volatile uint32_t errors_        = 0;
    volatile uint32_t errorCount_    = 0;
    volatile uint32_t errorTick_     = 0;
    volatile uint32_t resyncPending_ = 0;

    Status arm();
    void   scheduleResync();
    void   parseControlPacket();
} ;
