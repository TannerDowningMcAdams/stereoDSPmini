#include "tempo.h"
#include "spi_protocol.h"

// A tempo is a period and one beat instant. Phase anywhere follows from the two,
// so a tap sets its phase retroactively by making its press edge the beat.
typedef struct {
    uint32_t periodUs;      // 0 = none
    uint32_t beatUs;
} Beat;

static Beat internal;
static Beat midi;
static bool followMidi = true;

static bool     tapActive;  // a tap within TEMPO_TAP_RESET_US, so the next one measures
static uint32_t lastTapUs;
static uint32_t tapInterval[TEMPO_TAP_INTERVALS];
static uint8_t  tapCount;
static uint8_t  tapNext;

// The last TEMPO_CLOCK_PPQN + 1 clock times: one beat of intervals, a moving average
// that keeps byte-timing jitter out of the tempo.
static uint32_t clockUs[TEMPO_CLOCK_PPQN + 1u];
static uint8_t  clockCount; // valid entries
static uint8_t  clockNext;
static uint8_t  clockTick;  // position in the beat; 0 = on the beat

static uint32_t clampPeriod(uint32_t periodUs)
{
    if (periodUs < TEMPO_MIN_PERIOD_US) { return TEMPO_MIN_PERIOD_US; }
    if (periodUs > TEMPO_MAX_PERIOD_US) { return TEMPO_MAX_PERIOD_US; }
    return periodUs;
}

static void resetClock(void)
{
    clockCount = 0u;
    clockNext  = 0u;
    // The next clock is a beat.
    clockTick  = TEMPO_CLOCK_PPQN - 1u;
    midi.periodUs = 0u;
}

void tempoInit(void)
{
    internal   = (Beat) { 0u, 0u };
    midi       = (Beat) { 0u, 0u };
    followMidi = true;
    tapActive  = false;
    tapCount   = 0u;
    tapNext    = 0u;
    resetClock();
}

void tempoTap(uint32_t pressUs)
{
    const uint32_t interval = pressUs - lastTapUs;
    if (tapActive && interval <= TEMPO_TAP_RESET_US)
    {
        tapInterval[tapNext] = interval;
        tapNext = (uint8_t) ((tapNext + 1u) % TEMPO_TAP_INTERVALS);
        if (tapCount < TEMPO_TAP_INTERVALS) { tapCount++; }

        uint32_t sum = 0u;
        for (uint8_t i = 0; i < tapCount; i++) { sum += tapInterval[i]; }
        internal.periodUs = clampPeriod(sum / tapCount);
    }
    else
    {
        tapCount = 0u;
        tapNext  = 0u;
    }
    tapActive = true;
    lastTapUs = pressUs;
    internal.beatUs = pressUs;
}

void tempoRecall(uint16_t centiBpm, uint32_t nowUs)
{
    if (centiBpm == 0u) { return; }
    // 6e9 us per centi-BPM overflows 32 bits, so half of it is divided and doubled.
    internal.periodUs = clampPeriod((3000000000u / centiBpm) * 2u);
    internal.beatUs   = nowUs;
    tapActive = false;
}

void tempoMidiClock(uint32_t nowUs)
{
    clockUs[clockNext] = nowUs;
    clockNext = (uint8_t) ((clockNext + 1u) % (TEMPO_CLOCK_PPQN + 1u));
    if (clockCount < TEMPO_CLOCK_PPQN + 1u) { clockCount++; }

    if (clockCount >= 2u)
    {
        // clockNext now indexes the oldest entry once the ring is full.
        const uint8_t oldest = (clockCount == TEMPO_CLOCK_PPQN + 1u) ? clockNext : 0u;
        const uint32_t span = nowUs - clockUs[oldest];
        const uint32_t intervals = clockCount - 1u;
        midi.periodUs = clampPeriod(span / intervals * TEMPO_CLOCK_PPQN);
    }

    clockTick = (uint8_t) ((clockTick + 1u) % TEMPO_CLOCK_PPQN);
    if (clockTick == 0u) { midi.beatUs = nowUs; }
}

void tempoMidiStart(void)
{
    clockTick = TEMPO_CLOCK_PPQN - 1u;
}

void tempoSetFollowMidi(bool follow)
{
    followMidi = follow;
}

// Keeps the beat instant within one period of now, so differences never wrap.
static void advanceBeat(Beat* beat, uint32_t nowUs)
{
    if (beat->periodUs == 0u) { return; }
    const int32_t elapsed = (int32_t) (nowUs - beat->beatUs);
    if (elapsed >= (int32_t) beat->periodUs)
    {
        beat->beatUs += ((uint32_t) elapsed / beat->periodUs) * beat->periodUs;
    }
}

void tempoPoll(uint32_t nowUs)
{
    if (tapActive && (nowUs - lastTapUs) > TEMPO_TAP_RESET_US) { tapActive = false; }

    if (clockCount > 0u)
    {
        const uint8_t newest = (uint8_t) ((clockNext + TEMPO_CLOCK_PPQN) % (TEMPO_CLOCK_PPQN + 1u));
        // Tempo falls back to the internal value, which the clock never overwrites.
        if ((nowUs - clockUs[newest]) > TEMPO_CLOCK_LOSS_US) { resetClock(); }
    }

    advanceBeat(&internal, nowUs);
    advanceBeat(&midi, nowUs);
}

static const Beat* activeBeat(void)
{
    if (followMidi && midi.periodUs != 0u) { return &midi; }
    if (internal.periodUs != 0u)           { return &internal; }
    return 0;
}

uint8_t tempoSource(void)
{
    const Beat* beat = activeBeat();
    if (beat == 0)         { return TEMPO_SRC_NONE; }
    return (beat == &midi) ? TEMPO_SRC_MIDI : TEMPO_SRC_INTERNAL;
}

uint32_t tempoPeriodUs(void)
{
    const Beat* beat = activeBeat();
    return (beat == 0) ? 0u : beat->periodUs;
}

uint16_t tempoPhaseAt(uint32_t atUs)
{
    const Beat* beat = activeBeat();
    if (beat == 0) { return 0u; }

    const uint32_t period  = beat->periodUs;
    const int32_t  elapsed = (int32_t) (atUs - beat->beatUs);
    uint32_t rem;
    if (elapsed >= 0) { rem = (uint32_t) elapsed % period; }
    else
    {
        rem = period - ((uint32_t) -elapsed % period);
        if (rem == period) { rem = 0u; }
    }

    // rem < 2^21, so rem << 11 fits; the divisor loses under 0.1 %. Avoids a 64-bit
    // division, which the M0+ does in software.
    const uint32_t phase = (rem << 11) / (period >> 5);
    return (phase > 0xFFFFu) ? 0xFFFFu : (uint16_t) phase;
}
