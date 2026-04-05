#pragma once

#include "frame.hpp"
#include "frame_buffer.hpp"
#include "status.hpp"
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
    static constexpr uint16_t kFrameBufferSize = kBufferSize / 2;
    // 24.615385 MHz / (256 * (1+OSR)) = 48076.92382813 where OSR = 1
    static constexpr uint16_t kSampleRate = 48077;

    Status status_ = Status::INIT;

    void init();

    void rxHalfComplete();
    void rxComplete();
    void txHalfComplete();
    void txComplete();
    void saiErrorHandler();

    //FloatFrame* getInputBuffer()    { return inputBufferPointer_; }
    //FloatFrame* getOuputBuffer()    { return inputBufferPointer_; }
    FrameBuffer getInputBuffer();
    FrameBuffer getOutputBuffer();
    
private:
    SAI_HandleTypeDef *txHandle_ = &hsai_BlockA1;
    SAI_HandleTypeDef *rxHandle_ = &hsai_BlockB1;

    bool receiveReady_ = false;
    bool transmitReady_ = false;

    // DMA buffers for SAI transmission and reception
    // Instantiated in .cpp
    static volatile int32_t audioAdcDataDMA_[kCodecBufferSize];
    static volatile int32_t audioDacDataDMA_[kCodecBufferSize];
    static volatile int32_t *audioInPointer_;
    static volatile int32_t *audioOutPointer_;

    // Cached copy buffers for packing and unpacking
    // Half the size of the DMA buffers (not double buffered)
    int32_t audioAdcDataCache_[kBufferSize];
    int32_t audioDacDataCache_[kBufferSize];

    FloatFrame inputBuffer_[kFrameBufferSize];
    FloatFrame outputBuffer_[kFrameBufferSize];

    static constexpr float kInt24ToFloat = 1.0f / (1 << 23);
    static constexpr float kFloatToInt24 = (1 << 23);

    void packUnpackAudioData();
    void audioInitErrorHandler();
    void resetCodec();

} ;