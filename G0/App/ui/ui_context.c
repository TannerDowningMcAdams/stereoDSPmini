#include "ui_context.h"
#include "board.h"
#include "boot_state.h"
#include "engine_manifest.h"
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
static uint8_t  engine;
static uint8_t  presetIndex;
static uint16_t param[SPI_PARAM_COUNT];
static uint16_t discrete;
static uint16_t ownerMask;

static const EngineManifest* manifest(void)
{
    return &kEngineManifest[engine];
}

static uint8_t fieldGet(uint8_t field)
{
    const DiscreteField* f = &manifest()->field[field];
    return (uint8_t) ((discrete >> f->offset) & ((1u << f->width) - 1u));
}

static void fieldSet(uint8_t field, uint8_t value)
{
    const DiscreteField* f = &manifest()->field[field];
    const uint16_t mask = (uint16_t) (((1u << f->width) - 1u) << f->offset);
    discrete = (uint16_t) ((discrete & ~mask) | ((value << f->offset) & mask));
}

void uiInit(void)
{
    onLed  = boardPwmLedIndex(SWITCH_ROLE_ON);
    auxLed = boardPwmLedIndex(SWITCH_ROLE_AUX);
    bootDeadline = deadlineSet(BOOT_ECHO_WAIT_MS);
}

// Cold boot, or a warm reset the H7 cannot restore: manifest defaults, bypassed.
// The favourite preset replaces the defaults in M4.
static void enterDefaults(void)
{
    engine = kBoard.engines[0];
    presetIndex = 0u;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = manifest()->paramDefault[i]; }
    discrete  = engineDiscreteDefault(manifest());
    ownerMask = 0u;
    potRebaseline();
    context = CONTEXT_BYPASS;
}

// Warm reset: resume from the state the H7 last applied (plan §4.3).
static void adoptEcho(const H7ToG0Packet* echo)
{
    engine      = echo->activeEngine;
    presetIndex = echo->presetIndex;
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
    if (echo != NULL && (echo->h7Flags & H7_FLAG_STATE_VALID) != 0u && echo->activeEngine < ENGINE_COUNT)
    {
        adoptEcho(echo);
    }
    else if (deadlineExpired(bootDeadline))
    {
        enterDefaults();
    }
}

static void stepMode(int8_t direction)
{
    const uint8_t options = manifest()->modeOptions;
    if (options == 0u) { return; }
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

    // CONTEXT_RUN. ON, AUX and both holds are reserved for RECALL and STORE (M4).
    switch (gesture->type)
    {
        case GESTURE_ON_SHORT:
            context = CONTEXT_BYPASS;
            break;
        case GESTURE_AUX_SHORT:
            if (manifest()->auxRole == AUX_ROLE_TAP)
            {
                tempoTap(gesture->pressUs);
                tapThisTick = true;
            }
            else if (manifest()->auxRole == AUX_ROLE_TOGGLE)
            {
                fieldSet(manifest()->auxField, (uint8_t) (fieldGet(manifest()->auxField) ^ 1u));
            }
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
    if (run && manifest()->auxRole == AUX_ROLE_TOGGLE) { auxLit = fieldGet(manifest()->auxField) != 0u; }
    else                                               { auxLit = manifest()->usesTempo && beat; }
    ledPwmSet(onLed, run ? &on : &off);
    ledPwmSet(auxLed, auxLit ? &on : &off);

    uint8_t small = 0u;
    if (run && manifest()->modeOptions > 0u) { small = (uint8_t) (1u << fieldGet(ENGINE_MODE_FIELD)); }
    ledGpioSet(small);
}

void uiTick(void)
{
    if (context == CONTEXT_BOOT)
    {
        resolveBoot();
        if (context == CONTEXT_BOOT) { return; }
    }

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
    tx->engine      = engine;
    tx->presetIndex = presetIndex;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { tx->param[i] = param[i]; }
    tx->discrete     = discrete;
    tx->eventToggles = 0u;
    tx->tempoHz      = (period != 0u) ? 1000000.0f / (float) period : 0.0f;
    tx->tempoPhase   = (period != 0u) ? tempoPhaseAt(spiLinkNextEdgeUs()) : 0u;
    tx->ownerMask    = ownerMask;
    return true;
}
