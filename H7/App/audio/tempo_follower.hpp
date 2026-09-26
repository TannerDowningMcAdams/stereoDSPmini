#pragma once

#include <cstdint>

// Beat phase on the H7, locked to the G0's (plan §2, H4). The phase runs freely at
// tempoHz and is corrected toward the G0's tempoPhase, which is the phase at the CS
// rising edge of the frame that carried it. Both instants are cycle-counter stamps,
// so the correction does not depend on when the audio thread gets to it. No HAL.
class TempoFollower {
public:

    struct Config
    {
        uint32_t sampleRate;
        uint32_t cyclesPerSecond;   // rate of the cycle counter behind the timestamps
    };

    // An error larger than this is a new beat position (a tap or a recall) rather
    // than drift, and is taken at once.
    static constexpr float kSnapSeconds = 0.002f;
    // Fraction of a small error removed per frame. At 1 kHz frames this settles in
    // tens of ms, which averages out timestamp jitter and absorbs the clock mismatch
    // between the boards.
    static constexpr float kGain = 0.05f;

    TempoFollower() = default;
    ~TempoFollower() = default;

    void init(const Config& config);

    // Audio thread, when a control set lands. tempoHz 0 stops the phase. blockCycles
    // stamps the start of the block about to be processed, and the phase describes
    // that instant. A repeated frameSeq is a resent frame whose phase is stale.
    void update(float tempoHz, uint16_t phase, uint32_t edgeCycles, uint16_t frameSeq,
                uint32_t blockCycles);

    // Audio thread, once per block after any update. Returns the sample index at which
    // a beat falls in this block, or -1, and moves the phase to the next block.
    int32_t advance(uint16_t frames);

    bool  running() const { return running_; }
    float tempoHz() const { return tempoHz_; }
    // 0..1, at the start of the next block.
    float phase()   const { return phase_; }

private:

    float    sampleRate_      = 48000.0f;
    float    cyclesPerSecond_ = 1.0f;

    bool     running_  = false;
    float    tempoHz_  = 0.0f;
    float    increment_ = 0.0f;     // phase per sample
    float    phase_    = 0.0f;
    uint16_t lastSeq_  = 0;
    // A correction that moves the phase back across a beat must not sound it twice.
    // Saturates, well above any hold-off, so it cannot wrap.
    static constexpr uint32_t kSamplesSinceBeatMax = 1u << 30;
    uint32_t samplesSinceBeat_ = kSamplesSinceBeatMax;
};
