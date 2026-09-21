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
        // Slave with software NSS. Framing comes from the G0's CS line instead,
        // through its EXTI: see onFrameEnd().
        SPI_HandleTypeDef* spi;
    };

    G0Spi() = default;
    ~G0Spi() = default;

    static constexpr uint16_t kSpiPacketWords = SPI_PACKET_NUM_WORDS;

    void init(const Config& config);

    // SPI ISR: true only when a valid control packet updated params(). Anything
    // else leaves the last good set, or the defaults, in force.
    bool txRxComplete();
    // SPI ISR: latch the error. The next frame end re-arms, as it always does.
    void spiErrorHandler();
    // EXTI ISR on the G0's CS rising edge. The bus is idle until the next packet,
    // so this is the only place a transfer is ever armed.
    void onFrameEnd();

    const uiParams& params() const { return params_; }
    Status   status()     const { return status_; }
    uint32_t errorCount() const { return errorCount_; }
    // Accumulated HAL_SPI_ERROR_* bits since boot.
    uint32_t errors()     const { return errors_; }
    // CS rising edges seen, and how many found a transfer that began mid-packet.
    uint32_t frameCount()  const { return frameCount_; }
    uint32_t resyncCount() const { return resyncCount_; }
    
private:

    Config   config_ {};
    uiParams params_ {};
    volatile Status status_ = Status::INIT;

    static constexpr uint16_t kControlPacketId = 0x5A9E;
    static constexpr float    int12ToFloat     = 1.0f / 4095.0f;

    // DMA buffers for SPI transmission and reception. Not volatile: the carve-out
    // is uncached, and volatile only invites a cast that strips it (UB).
    static SpiControlPacket rxPacketDMA_;
    static SpiControlPacket txPacketDMA_;

    // Every writer runs at priority 2 (SPI2, both DMA streams, EXTI15_10), so they
    // cannot preempt each other and need no critical section.
    volatile uint32_t errors_      = 0;
    volatile uint32_t errorCount_  = 0;
    volatile uint32_t frameCount_  = 0;
    volatile uint32_t resyncCount_ = 0;

    Status arm();
    void   parseControlPacket();
} ;
