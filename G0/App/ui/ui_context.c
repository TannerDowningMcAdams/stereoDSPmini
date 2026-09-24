#include "ui_context.h"
#include "board.h"
#include "boot_state.h"
#include "engine_link.h"
#include "gesture.h"
#include "led.h"
#include "pot.h"
#include "spi_link.h"
#include "tempo.h"
#include "timebase.h"

// The H7 answers the G0's first frame; this covers a few dropped ones.
#define BOOT_ECHO_WAIT_MS   50u

// Until settings (M5): true bypass, digital dry, stereo in. Build with
// RUN_FLAG_TRAILS and/or RUN_FLAG_ANALOG_DRY added to test the other modes.
#define DEFAULT_RUN_FLAGS   (RUN_FLAG_STEREO_IN)

#define LED_FULL            255u

typedef enum {
    CONTEXT_BOOT,
    CONTEXT_BYPASS,
    CONTEXT_RUN
} UiContext;

static UiContext context = CONTEXT_BOOT;
static uint32_t  bootDeadline;
static uint8_t   onLed;
static uint8_t   auxLed;

// Tempo LED. A blink starts where the beat phase wraps, and lasts a quarter beat.
static uint16_t  lastBeatPhase;
static uint32_t  blinkStartUs;
static bool      tapThisTick;

// Live state, sent every frame.
static uint16_t engineId;           // wanted engine; ENGINE_ID_NONE until the H7 names one
static uint8_t  presetIndex;
static uint16_t param[SPI_PARAM_COUNT];
static uint16_t discrete;
static uint16_t ownerMask;

// Engine requests (plan §4.4). A query asks the H7 for the id at a registry index.
// A defaults request asks it for the wanted engine's defaults, which the G0 adopts
// from the echo; until then the H7 ignores param[] and discrete.
static uint8_t  engineQuery;
static bool     queryPending;
static uint8_t  defaultsSeq;
static bool     defaultsPending;

// The wanted engine's descriptor, as last echoed while it ran. engineReady: the echo
// reports it running and the G0 holds its values, so its fields may be edited.
static uint32_t engineDesc;
static bool     engineReady;

void uiInit(void)
{
    onLed  = boardPwmLedIndex(SWITCH_ROLE_ON);
    auxLed = boardPwmLedIndex(SWITCH_ROLE_AUX);
    bootDeadline = deadlineSet(BOOT_ECHO_WAIT_MS);
}

static void requestEngineAt(uint8_t index)
{
    engineQuery  = index;
    queryPending = true;
}

// Cold boot, or a warm reset the H7 cannot restore: registry index 0 with its
// defaults, bypassed. The favourite preset replaces this in M4.
static void enterDefaults(void)
{
    engineId    = ENGINE_ID_NONE;
    presetIndex = 0u;
    ownerMask   = 0u;
    requestEngineAt(0u);
    context = CONTEXT_BYPASS;
}

// Warm reset: resume from the state the H7 last applied (plan §4.3).
static void adoptEcho(const H7ToG0Packet* echo)
{
    engineId        = echo->activeEngine;
    defaultsSeq     = echo->defaultsSeqEcho;
    defaultsPending = false;
    presetIndex     = echo->presetIndex;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = echo->param[i]; }
    discrete    = echo->discrete;
    ownerMask   = echo->ownerMask;
    potRebaseline();
    // No phase in the echo, so the beat restarts now; a MIDI tempo returns with the clock.
    // Period and signed cast: a float multiply or unsigned cast links 0.7-2 KB more soft-float.
    const uint16_t tempoSrc = (uint16_t) ((echo->runFlags & RUN_FLAG_TEMPO_SRC_MASK) >> RUN_FLAG_TEMPO_SRC_SHIFT);
    if (tempoSrc == TEMPO_SRC_INTERNAL && echo->tempoHz > 0.0f)
    {
        const uint32_t periodUs = (uint32_t) (int32_t) (1000000.0f / echo->tempoHz);
        if (periodUs >= TEMPO_MIN_PERIOD_US / 2u)
        {
            tempoRecall((uint16_t) ((3000000000u / periodUs) * 2u), timebaseNowUs());
        }
    }
    context = ((echo->runFlags & RUN_FLAG_ENGAGED) != 0u) ? CONTEXT_RUN : CONTEXT_BYPASS;
}

static void resolveBoot(void)
{
    if (bootWasCold()) { enterDefaults(); return; }

    const H7ToG0Packet* echo = spiLinkEcho();
    const uint16_t ready = H7_FLAG_STATE_VALID | H7_FLAG_ENGINE_READY;
    if (echo != NULL && (echo->h7Flags & ready) == ready)
    {
        adoptEcho(echo);
    }
    else if (deadlineExpired(bootDeadline))
    {
        enterDefaults();
    }
}

// Follows the H7's answers to the engine requests, once per tick.
static void trackEngine(void)
{
    const H7ToG0Packet* echo = spiLinkEcho();
    engineReady = false;
    if (echo == NULL) { return; }

    // The registry is fixed, so an answer for this index is current even if it was
    // sent for an earlier query.
    if (queryPending && echo->engineQueryEcho == engineQuery)
    {
        queryPending = false;
        if (echo->engineQueryId != ENGINE_ID_NONE)
        {
            engineId = echo->engineQueryId;
            defaultsSeq++;
            defaultsPending = true;
        }
    }

    // An id the H7 does not have, e.g. from a preset stored under older firmware:
    // keep the engine it runs.
    if ((echo->h7Flags & H7_FLAG_ENGINE_UNKNOWN) != 0u && !queryPending && echo->activeEngine != engineId)
    {
        engineId        = echo->activeEngine;
        defaultsPending = false;
    }

    const bool active = echo->activeEngine == engineId && (echo->h7Flags & H7_FLAG_ENGINE_READY) != 0u;
    if (!active) { return; }
    engineDesc = echo->engineDesc;

    if (defaultsPending)
    {
        if (echo->defaultsSeqEcho != defaultsSeq) { return; }
        for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = echo->param[i]; }
        discrete        = echo->discrete;
        ownerMask       = 0u;
        defaultsPending = false;
        potRebaseline();
    }
    engineReady = true;
}

// Until ENGINE select (M4): the next engine in the H7's registry, with its defaults.
static void stepEngine(void)
{
    const H7ToG0Packet* echo = spiLinkEcho();
    if (echo == NULL || !engineReady || echo->engineCount == 0u) { return; }
    uint8_t next = (uint8_t) (echo->activeIndex + 1u);
    if (next >= echo->engineCount) { next = 0u; }
    requestEngineAt(next);
}

static uint8_t fieldGet(uint8_t field)
{
    return engineFieldGet(engineDesc, discrete, field);
}

static void fieldSet(uint8_t field, uint8_t value)
{
    discrete = engineFieldSet(engineDesc, discrete, field, value);
}

static void stepMode(int8_t direction)
{
    const uint8_t options = engineDescModeOptions(engineDesc);
    if (!engineReady || options == 0u) { return; }
    const uint8_t value = fieldGet(ENGINE_MODE_FIELD);
    // Clamped, so a held switch always lands at a known end.
    if (direction > 0 && value + 1u < options) { fieldSet(ENGINE_MODE_FIELD, (uint8_t) (value + 1u)); }
    if (direction < 0 && value > 0u)           { fieldSet(ENGINE_MODE_FIELD, (uint8_t) (value - 1u)); }
}

static void handleGesture(const Gesture* gesture)
{
    if (context == CONTEXT_BYPASS)
    {
        if (gesture->type == GESTURE_ON_SHORT) { context = CONTEXT_RUN; }
        return;
    }

    // CONTEXT_RUN. ON and both holds are reserved for RECALL and STORE (M4).
    const uint8_t auxRole = engineDescAuxRole(engineDesc);
    switch (gesture->type)
    {
        case GESTURE_ON_SHORT:
            context = CONTEXT_BYPASS;
            break;
        case GESTURE_AUX_SHORT:
            if (auxRole == AUX_ROLE_TAP)
            {
                tempoTap(gesture->pressUs);
                tapThisTick = true;
            }
            else if (auxRole == AUX_ROLE_TOGGLE && engineReady)
            {
                const uint8_t field = engineDescAuxField(engineDesc);
                fieldSet(field, (uint8_t) (fieldGet(field) ^ 1u));
            }
            break;
        case GESTURE_AUX_HOLD:
            stepEngine();
            break;
        case GESTURE_MODE_UP:
            stepMode(1);
            break;
        case GESTURE_MODE_DOWN:
            stepMode(-1);
            break;
        default:
            break;
    }
}

static void applyPots(void)
{
    for (uint8_t i = 0; i < kBoard.numPots; i++)
    {
        if (!potMoved(i)) { continue; }
        const uint8_t index = kBoard.pots[i].paramIndex;
        param[index] = potValue(i);
        ownerMask = (uint16_t) (ownerMask | (1u << index));
    }
}

// True while the tempo LED should be lit. Called every tick to track the phase.
static bool beatLit(void)
{
    const uint32_t period = tempoPeriodUs();
    const uint32_t now    = timebaseNowUs();
    const uint16_t phase  = (period != 0u) ? tempoPhaseAt(now) : 0u;
    const bool wrapped = phase < lastBeatPhase;
    lastBeatPhase = phase;

    // A tap moves the beat onto its press, which also reads as a wrap. The LED waits
    // for the next beat of the new grid instead, and a grid that shifts within half a
    // beat of the last blink does not blink again.
    if (wrapped && !tapThisTick && (now - blinkStartUs) >= period / 2u) { blinkStartUs = now; }
    tapThisTick = false;
    return period != 0u && (now - blinkStartUs) < period / 4u;
}

static void renderLeds(void)
{
    const LedPattern off = { LED_OFF, 0u, 0u, 0u };
    const LedPattern on  = { LED_ON, LED_FULL, 0u, 0u };
    const bool run = context == CONTEXT_RUN;

    // A toggle field shows its state in RUN. Otherwise the AUX LED blinks with the
    // tempo, in BYPASS too, whenever the engine uses one and one is set.
    const bool beat = beatLit();
    bool auxLit;
    if (run && engineDescAuxRole(engineDesc) == AUX_ROLE_TOGGLE)
    {
        auxLit = fieldGet(engineDescAuxField(engineDesc)) != 0u;
    }
    else
    {
        auxLit = engineDescUsesTempo(engineDesc) && beat;
    }
    ledPwmSet(onLed, run ? &on : &off);
    ledPwmSet(auxLed, auxLit ? &on : &off);

    uint8_t small = 0u;
    if (run && engineReady && engineDescModeOptions(engineDesc) > 0u)
    {
        small = (uint8_t) (1u << fieldGet(ENGINE_MODE_FIELD));
    }
    ledGpioSet(small);
}

void uiTick(void)
{
    if (context == CONTEXT_BOOT)
    {
        resolveBoot();
        if (context == CONTEXT_BOOT) { return; }
    }

    trackEngine();

    Gesture gesture;
    const uint32_t nowUs = timebaseNowUs();
    while (gestureNext(nowUs, &gesture)) { handleGesture(&gesture); }

    applyPots();
    renderLeds();
}

bool uiFillFrame(G0ToH7Packet* tx)
{
    if (context == CONTEXT_BOOT) { return false; }

    const uint32_t period = tempoPeriodUs();
    tx->runFlags    = (uint16_t) (DEFAULT_RUN_FLAGS | ((context == CONTEXT_RUN) ? RUN_FLAG_ENGAGED : 0u) |
                                  (tempoSource() << RUN_FLAG_TEMPO_SRC_SHIFT));
    tx->engineId    = engineId;
    tx->presetIndex = presetIndex;
    tx->engineQuery = engineQuery;
    tx->defaultsSeq = defaultsSeq;
    tx->g0Flags     = defaultsPending ? G0_FLAG_DEFAULTS_PENDING : 0u;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { tx->param[i] = param[i]; }
    tx->discrete     = discrete;
    tx->eventToggles = 0u;
    tx->tempoHz      = (period != 0u) ? 1000000.0f / (float) period : 0.0f;
    tx->tempoPhase   = (period != 0u) ? tempoPhaseAt(spiLinkNextEdgeUs()) : 0u;
    tx->ownerMask    = ownerMask;
    return true;
}
