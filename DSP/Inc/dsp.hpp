#define DSP_USE_CMSIS

#ifdef DSP_USE_CMSIS
#include "arm_math.h"
#else
// TODO: #include std lib, eigen, etc. for JUCE compilation
#endif


namespace dsp {

    namespace math {
    
        float rms(const float* src, uint32_t len);
        void vectorAdd(const float* a, const float* b, float* out, uint32_t len);
        bool matMul(const float* a, uint16_t aRows, uint16_t aCols,
                const float* b, uint16_t bRows, uint16_t bCols,
                float* c);

    }

    class Effect;
    class AudioBuffer;
    
}