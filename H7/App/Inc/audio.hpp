#pragma once

#include "status_hal.hpp"
#include "audio_buffer.hpp"
#include "processor.hpp"
#include "pin.hpp"
#include <cstdint>

class Audio {
public:

    struct Config
    {
        // dac must be the clock master: only its callbacks publish the half, and
        // adc must be synchronous to it.
        SAI_HandleTypeDef* dac;
        SAI_HandleTypeDef* adc;
        Pin                codecReset;
    };

    // Index into the per-block SAI error latches.
    enum class SaiId : uint8_t { DAC = 0, ADC = 1, COUNT = 2 };

    Audio() = default;
    ~Audio() = default;

    // kBlockSize is the root: the frame count the DSP sees. The DMA ring is two
    // halves of one block, each kAudioChannels words per frame.
    static constexpr uint16_t kBlockSize       = 32;
    static constexpr uint16_t kAudioChannels   = 2;
    static constexpr uint16_t kNumHalves       = 2;
    static constexpr uint16_t kHalfWords       = kBlockSize * kAudioChannels;
    static constexpr uint16_t kCodecBufferSize = kHalfWords * kNumHalves;
    // 24.615385 MHz / (256 * (1+OSR)) = 48076.92382813 where OSR = 1
    static constexpr uint32_t kSampleRate = 48077;

    // Bind before init(): serviceBlock() hands every block straight to it.
    void setProcessor(Processor* p) { processor_ = p; }

    void init(const Config& config);
    Status status() const { return status_; }

    // Thread mode, from the App_Run() loop: processes the newest block if one is
    // pending. The ISR only publishes; all DSP runs here.
    void serviceBlock();

    // Thread mode only: restarts the SAI if an error stopped it. The HAL calls
    // involved poll SysTick, which cannot preempt the priority-0 DMA IRQs.
    void serviceErrors();

    // Both blocks share one clock and frame, so only the master dac block's
    // callbacks publish the half; the adc block's are deliberately empty.
    void txHalfComplete();
    void txComplete();
    // Not masked at the DMA: HAL completes a stream abort on its TC interrupt,
    // so masking TCIE would leave an aborted adc stream unrecoverable.
    void rxHalfComplete() { }
    void rxComplete()     { }
    // ISR context, from SAI1_IRQn or a DMA stream IRQ. Latches and returns.
    void audioErrorHandler(SAI_HandleTypeDef* hsai);

    uint32_t errorCount()    const { return errorCount_; }
    // Blocks published by the master; blocks lost because serviceBlock() fell behind.
    uint32_t blockCount()    const { return blockCount_; }
    uint32_t blockOverruns() const { return blockOverruns_; }
    // Accumulated HAL_SAI_ERROR_* bits, and callback count, since the last clear.
    uint32_t saiError(SaiId id)      const { return saiErrors_[static_cast<uint8_t>(id)]; }
    uint32_t saiErrorCount(SaiId id) const { return saiErrorCounts_[static_cast<uint8_t>(id)]; }
    void     clearSaiErrors();
    
private:
    Config config_ {};
    Status status_ = Status::INIT;
    Processor* processor_ = nullptr;

    // Not volatile: the carve-out is uncached and the interrupt boundary is the
    // synchronisation. Declaring it volatile only invited a const_cast, which is UB.
    static int32_t audioAdcDataDMA_ [kCodecBufferSize];
    static int32_t audioDacDataDMA_ [kCodecBufferSize];

    // The DMA half the DSP owns: 0 after half-transfer, 1 after complete. Seeded
    // to 1 so the first half-transfer interrupt moves it to 0.
    static volatile uint32_t offset_;

    // blockCount_ and blockOverruns_ are written by the ISR, consumedCount_ by
    // serviceBlock(). Their difference is how far the DSP is behind.
    volatile uint32_t blockCount_    = 0;
    volatile uint32_t consumedCount_ = 0;
    volatile uint32_t blockOverruns_ = 0;
    // DWT cycle count when the latest block was published, written with the count.
    volatile uint32_t blockCycles_   = 0;

    // Cached copy buffers for packing and unpacking
    // Half the size of the DMA buffers (not double buffered)
    float leftInputBuffer_   [kBlockSize];
    float rightInputBuffer_  [kBlockSize];
    float leftOutputBuffer_  [kBlockSize];
    float rightOutputBuffer_ [kBlockSize];

    dsp::AudioBuffer inputBuffer_  {leftInputBuffer_, rightInputBuffer_, kBlockSize};
    dsp::AudioBuffer outputBuffer_ {leftOutputBuffer_, rightOutputBuffer_, kBlockSize};

    static constexpr float kInt24ToFloat = 1.0f / (1 << 23);
    static constexpr float kFloatToInt24 = (1 << 23);
    // 24-bit codec data sits left-aligned in each 32-bit DMA word.
    static constexpr uint16_t kDmaWordShift = 8;

    // HAL never clears hsai->ErrorCode while a transfer runs, so latch per block
    // and clear it on every callback: the count becomes a rate, the bits a union.
    volatile uint32_t saiErrors_      [static_cast<uint8_t>(SaiId::COUNT)] = {};
    volatile uint32_t saiErrorCounts_ [static_cast<uint8_t>(SaiId::COUNT)] = {};
    volatile uint32_t errorCount_     = 0;
    // Set by the ISR when an error stopped a transfer; consumed by serviceErrors().
    volatile uint32_t restartPending_ = 0;

    // Only OVR/UDR leave the transfer running; every other error stops a stream.
    static constexpr uint32_t kNonFatalErrors = HAL_SAI_ERROR_OVR | HAL_SAI_ERROR_UDR;

    // ISR side of the handoff: count the block, and an overrun if one was missed.
    void signalBlock();
    void resetCodec();
    Status startDMA();
    // Abort both blocks and re-arm. Thread mode only.
    Status restart();
    SaiId identify(const SAI_HandleTypeDef* hsai) const;

} ;
