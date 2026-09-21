#pragma once

#include "status.hpp"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_dma.h"
#include "audio_buffer.hpp"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "sai.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

class Audio {
public:

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

    Status status_ = Status::INIT;

    void init();

    // Thread mode only: restarts the SAI if an error stopped it. The HAL calls
    // involved poll SysTick, which cannot preempt the priority-0 DMA IRQs.
    void serviceErrors();

    // Both blocks share one clock and frame, so only master A1's callbacks
    // publish the half; B1's are deliberately empty.
    void txHalfComplete();
    void txComplete();
    // Not masked at the DMA: HAL completes a stream abort on its TC interrupt,
    // so masking TCIE would leave an aborted B1 stream unrecoverable.
    void rxHalfComplete() { }
    void rxComplete()     { }
    // ISR context, from SAI1_IRQn or a DMA stream IRQ. Latches and returns.
    void audioErrorHandler(SAI_HandleTypeDef* hsai);

    uint32_t errorCount() const { return errorCount_; }
    // Accumulated HAL_SAI_ERROR_* bits, and callback count, since the last clear.
    uint32_t saiError(SaiId id)      const { return saiErrors_[static_cast<uint8_t>(id)]; }
    uint32_t saiErrorCount(SaiId id) const { return saiErrorCounts_[static_cast<uint8_t>(id)]; }
    void     clearSaiErrors();

    dsp::AudioBuffer getInputBuffer()  { return inputBuffer_; }
    dsp::AudioBuffer getOutputBuffer() { return outputBuffer_; }
    
private:
    SAI_HandleTypeDef* txHandle_ = &hsai_BlockA1;
    SAI_HandleTypeDef* rxHandle_ = &hsai_BlockB1;

    // Not volatile: the carve-out is uncached and the interrupt boundary is the
    // synchronisation. Declaring it volatile only invited a const_cast, which is UB.
    static int32_t audioAdcDataDMA_ [kCodecBufferSize];
    static int32_t audioDacDataDMA_ [kCodecBufferSize];

    // The DMA half the DSP owns: 0 after half-transfer, 1 after complete. Seeded
    // to 1 so the first half-transfer interrupt moves it to 0.
    static volatile uint32_t offset_;

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

    void serviceBlock();
    void resetCodec();
    HAL_StatusTypeDef startDMA();
    // Abort both blocks and re-arm. Thread mode only.
    HAL_StatusTypeDef restart();
    SaiId identify(const SAI_HandleTypeDef* hsai) const;

} ;
