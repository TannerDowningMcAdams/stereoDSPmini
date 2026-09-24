#pragma once
#include "status_hal.hpp"
#include "engine_link.h"
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
        uint16_t           csPin;   // GPIO_PIN_x of the CS line; also its EXTI line
    };

    // The G0's state as last applied. param[] and discrete change only on frames that
    // want the running engine, so they never drive the wrong one, and not at all while
    // the G0 waits for defaults. The engine host sets them through setApplied().
    struct Controls
    {
        uint16_t engineId;      // the engine param[] and discrete belong to; ENGINE_ID_NONE while switching
        uint8_t  presetIndex;
        uint16_t runFlags;
        uint16_t param[SPI_PARAM_COUNT];
        uint16_t discrete;
        uint16_t eventToggles;
        float    tempoHz;
        uint16_t tempoPhase;
        uint16_t ownerMask;
        uint16_t frameSeq;
    };

    // What the G0 last asked for, for the engine host.
    struct Request
    {
        uint16_t engineId;
        uint8_t  defaultsSeq;
        bool     defaultsPending;           // G0_FLAG_DEFAULTS_PENDING
        uint16_t param[SPI_PARAM_COUNT];    // as sent, whatever the engine
        uint16_t discrete;
    };

    // The engine state the echo reports, published by the engine host.
    struct EngineStatus
    {
        uint32_t descriptor;
        uint16_t activeId;
        uint16_t flags;         // H7_FLAG_ENGINE_READY, _LOADING, _UNKNOWN
        uint8_t  activeIndex;
    };

    G0Spi() = default;
    ~G0Spi() = default;

    static constexpr uint16_t kSpiPacketWords = SPI_PACKET_NUM_WORDS;
    // Well above a G0 flash page erase (20-40 ms of dropped frames).
    static constexpr uint32_t kLinkTimeoutMs  = 100u;

    void init(const Config& config);

    // SPI ISR: true only when a valid frame updated controls(). Anything else leaves
    // the last good set, or the defaults, in force.
    bool txRxComplete();
    // SPI ISR: latch the error. The next frame end re-arms, as it always does.
    void spiErrorHandler();
    // EXTI ISR on the G0's CS rising edge. The bus is idle until the next packet,
    // so this is the only place a transfer is ever armed. True when the edge ended a
    // valid frame: controls() is then complete, and frameEdgeCycles() is its edge.
    bool onFrameEnd();

    // Thread mode. While set, every frame sent to the G0 carries the bootloader magic.
    void setBootloaderRequest(bool request) { bootloaderRequest_ = request; }
    // Thread mode. Runs onFrameEnd() through a software EXTI, so a transfer that was
    // armed with stale contents is replaced before the G0 clocks it out.
    void restartFrame();

    // Thread mode. The last valid frame's request; false before the first one.
    bool request(Request& out) const;
    // Thread mode. What the echo reports about the engine from the next frame on.
    void publishEngine(const EngineStatus& status);
    // Thread mode. Replaces the applied param[] and discrete, and echoes defaultsSeq,
    // at the next valid frame. False while the previous call has not been taken yet.
    bool setApplied(const uint16_t* param, uint16_t discrete, uint8_t defaultsSeq);

    // What the H7 applies before the G0's first valid frame: no engine's params.
    static Controls defaultControls();

    const Controls& controls() const { return controls_; }
    // A valid frame arrived within kLinkTimeoutMs. Losing the link changes nothing
    // else: the last good controls stay in force.
    bool linkUp() const;
    // DWT cycle count at the CS rising edge that ended the last valid frame.
    uint32_t frameEdgeCycles() const { return frameEdgeCycles_; }

    // From any CRC-valid G0 frame, whatever its protocol version, so the H7 can
    // detect a G0 that needs reprogramming.
    bool     g0Seen()            const { return g0Seen_; }
    uint16_t g0ProtocolVersion() const { return g0ProtocolVersion_; }
    uint16_t g0FwVersion()       const { return g0FwVersion_; }

    Status   status()     const { return status_; }
    uint32_t errorCount() const { return errorCount_; }
    // Accumulated HAL_SPI_ERROR_* bits since boot.
    uint32_t errors()     const { return errors_; }
    // CS rising edges seen, and how many found a transfer that began mid-packet.
    uint32_t frameCount()  const { return frameCount_; }
    uint32_t resyncCount() const { return resyncCount_; }
    uint32_t validFrameCount() const { return validFrameCount_; }

private:

    Config   config_ {};
    Controls controls_ {};
    volatile Status status_ = Status::INIT;

    // Written by the ISRs, copied by request() under validFrameCount_.
    Request  request_ {};
    uint8_t  engineQuery_     = 0;
    uint8_t  defaultsSeqEcho_ = 0;

    // Double-buffered: publishEngine() writes the buffer the ISRs are not reading, then
    // flips the index. Thread mode cannot preempt an ISR mid-read.
    EngineStatus     engineStatus_[2] {};
    volatile uint8_t engineStatusIndex_ = 0;

    // applied_ belongs to setApplied() while appliedReady_ is clear, and to parse()
    // while it is set.
    struct Applied
    {
        uint16_t param[SPI_PARAM_COUNT];
        uint16_t discrete;
        uint8_t  defaultsSeq;
    };
    Applied       applied_ {};
    volatile bool appliedReady_ = false;

    // DMA buffers, aligned for halfword DMA although the packets are packed. Not
    // volatile: the carve-out is uncached, and volatile only invites a cast that strips it (UB).
    alignas(4) static G0ToH7Packet rxPacketDMA_;
    alignas(4) static H7ToG0Packet txPacketDMA_;

    // Every writer below runs at priority 2 (SPI2, both DMA streams, EXTI15_10), so
    // they cannot preempt each other and need no critical section.
    volatile uint32_t errors_          = 0;
    volatile uint32_t errorCount_      = 0;
    volatile uint32_t frameCount_      = 0;
    volatile uint32_t resyncCount_     = 0;
    volatile uint32_t validFrameCount_ = 0;
    volatile uint32_t lastValidMs_     = 0;
    volatile uint32_t frameEdgeCycles_ = 0;
    bool              frameValid_      = false;   // set by txRxComplete(), taken by onFrameEnd()
    uint16_t          frameSeq_        = 0;

    volatile bool     g0Seen_            = false;
    volatile uint16_t g0ProtocolVersion_ = 0;
    volatile uint16_t g0FwVersion_       = 0;

    volatile bool     bootloaderRequest_ = false;

    Status arm();
    void   parse();
    void   buildTx();
} ;
