#pragma once
#include <cstdint>

namespace dsp {

// Linear trapezoidal state variable filter, 12 dB/octave, for one channel, after
// Andrew Simper's "Solving the continuous SVF equations using trapezoidal
// integration" (Cytomic, SvfLinearTrapOptimised2). Every response is a mix of the
// input and the bandpass and lowpass outputs of one core, so the mode only picks
// three coefficients. The cutoff is prewarped, and frequency, Q and gain can change
// every block without clicks.
class Svf {
public:

    enum class Mode : uint8_t
    {
        Lowpass, Highpass,
        Bandpass,       // unity gain at the centre
        Notch,
        Peak,           // highpass minus lowpass: a resonant boost around the cutoff
        Allpass,
        Bell,           // gainDb at the centre; Q sets the width
        LowShelf,       // gainDb below the cutoff
        HighShelf,      // gainDb above the cutoff
    };

    struct Config
    {
        uint32_t sampleRate;
        Mode     mode;
        float    frequency;     // Hz
        float    q;             // 0.7071 for a Butterworth lowpass or highpass
        float    gainDb;        // Bell and shelves only
    };

    static constexpr float kMaxRatio = 0.49f;   // of the sample rate
    static constexpr float kMinQ     = 0.1f;
    static constexpr float kMaxQ     = 40.0f;
    static constexpr float kMaxDb    = 40.0f;

    // Starts from silence.
    void init(const Config& config);

    // Each setter recomputes the coefficients. setFrequency costs a tanf and
    // setGainDb a powf, so set() takes all three for one update when they change
    // together. Values are clamped to the limits above.
    void setFrequency(float hz);
    void setQ(float q);
    void setGainDb(float db);
    void setMode(Mode mode);
    void set(float hz, float q, float db);

    void reset();

    float process(float input)
    {
        const float v3 = input - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;           // bandpass
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;  // lowpass
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return m0_ * input + m1_ * v1 + m2_ * v2;
    }

    // output may equal input.
    void process(const float* input, float* output, uint16_t frames);

    float frequency() const { return hz_; }
    float q()         const { return q_; }
    float gainDb()    const { return db_; }
    Mode  mode()      const { return mode_; }

private:

    void setFrequencyOnly(float hz);
    void setGainOnly(float db);
    void update();

    float piOverRate_ = 0.0f;
    float maxHz_      = 0.0f;
    float hz_         = 0.0f;
    float q_          = 0.7071f;
    float db_         = 0.0f;
    float tan_        = 0.0f;   // tan(pi * hz / rate)
    float amp_        = 1.0f;   // 10^(db / 40): the square root of the linear gain
    float sqrtAmp_    = 1.0f;
    Mode  mode_       = Mode::Lowpass;

    float a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float m0_ = 0.0f, m1_ = 0.0f, m2_ = 1.0f;
    float ic1eq_ = 0.0f;
    float ic2eq_ = 0.0f;
};

} // namespace dsp
