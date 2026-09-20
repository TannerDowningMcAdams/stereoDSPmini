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

    // Both blocks share one clock and frame, so four sets of block interrupts
    // raced. Only master A1 keeps its own; startDMA() masks B1's.
    void txHalfComplete();
    void txComplete();
    // Masked in startDMA(), so unreachable. Present only because callbacks.cpp
    // overrides the HAL weak symbols.
    void rxHalfComplete() { }
    void rxComplete()     { }
    void audioErrorHandler();

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

    uint32_t errorState_ = 0;
    uint32_t errorCount_ = 0;

    void serviceBlock();
    void initErrorHandler();
    void recoverFromError(uint32_t saiErrorCode);
    void resetCodec();
    HAL_StatusTypeDef startDMA();
    // Drop a channel's half/complete interrupts, leaving its error
    // interrupts armed.
    static void maskBlockInterrupts(DMA_HandleTypeDef* hdma);

} ;
