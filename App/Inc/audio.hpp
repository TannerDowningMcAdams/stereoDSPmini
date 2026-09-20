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

    static constexpr uint16_t kCodecBufferSize = 128;
    static constexpr uint16_t kBufferSize = kCodecBufferSize / 2;
    static constexpr uint16_t kBlockSize = kBufferSize / 2;
    // 24.615385 MHz / (256 * (1+OSR)) = 48076.92382813 where OSR = 1
    static constexpr uint16_t kSampleRate = 48077;

    Status status_ = Status::INIT;

    void init();

    void rxHalfComplete();
    void rxComplete();
    void txHalfComplete();
    void txComplete();
    void audioErrorHandler();

    dsp::AudioBuffer getInputBuffer()  { return inputBuffer_; }
    dsp::AudioBuffer getOutputBuffer() { return outputBuffer_; }
    
private:
    SAI_HandleTypeDef* txHandle_ = &hsai_BlockA1;
    SAI_HandleTypeDef* rxHandle_ = &hsai_BlockB1;
    //DMA_HandleTypeDef* dmaTxHandle_ = &hdma_sai1_a;
    //DMA_HandleTypeDef* dmaRxHandle_ = &hdma_sai1_b;

    bool receiveReady_  =  false;
    bool transmitReady_ =  false;

    // DMA buffers for SAI transmission and reception
    // Instantiated in .cpp
    static volatile int32_t audioAdcDataDMA_ [kCodecBufferSize];
    static volatile int32_t audioDacDataDMA_ [kCodecBufferSize];
    static volatile int32_t *audioInPointer_;
    static volatile int32_t *audioOutPointer_;

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

    uint32_t errorState_ = 0;
    uint32_t errorCount_ = 0;

    void packUnpackAudioData();
    void initErrorHandler();
    void recoverFromError(uint32_t saiErrorCode);
    void resetCodec();
    HAL_StatusTypeDef startDMA();

} ;