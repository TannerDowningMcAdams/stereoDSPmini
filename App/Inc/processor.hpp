#pragma once

#include "audio_buffer.hpp"
#ifdef __cplusplus
extern "C" {
#endif
#include "frame.hpp"
#include "audio_buffer.hpp"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class Processor {
public:

    Processor() = default;
    ~Processor() = default;

    //enum class AudioStatus { BUSY, READY, ERROR };

    void init();

    void processAudio(AudioBuffer input, AudioBuffer output, );
    
private:
    SAI_HandleTypeDef *txHandle_ = &hsai_BlockA1;
    SAI_HandleTypeDef *rxHandle_ = &hsai_BlockB1;

    static constexpr uint16_t kCodecBufferSize = 128;
    static constexpr uint16_t kBufferSize = kCodecBufferSize / 2;
    static constexpr uint16_t kFrameBufferSize = kBufferSize / 2;
    // SAI clock = 24.615385 MHz -> / (256 * (1+OSR)) where OSR = 1
    static constexpr float kSampleRate = 48076.92382813f;

    bool receiveReady_ = false;
    bool transmitReady_ = false;

    // DMA buffers for SAI transmission and reception
    static volatile int32_t audioAdcDataDMA_[kCodecBufferSize];
    static volatile int32_t audioDacDataDMA_[kCodecBufferSize];

    // Cached copy buffers for packing and unpacking
    // Half size of the DMA buffers to allow for double buffering
    int32_t audioAdcDataCache_[kBufferSize];
    int32_t audioDacDataCache_[kBufferSize];

    static volatile int32_t *audioInPointer_;
    static volatile int32_t *audioOutPointer_;

    FloatFrame inputBuffer_[kFrameBufferSize];
    FloatFrame outputBuffer_[kFrameBufferSize];

    FloatFrame* inputBufferPointer_;
    FloatFrame* outputBufferPointer_;

    static constexpr float kInt24ToFloat = 1.0f / (1 << 23);
    static constexpr float kFloatToInt24 = (1 << 23);

    void packUnpackAudioData();
    void audioInitErrorHandler();
    void resetCodec();
} ;