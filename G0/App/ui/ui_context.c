#include "ui_context.h"
#include "board.h"
#include "boot_state.h"
#include "engine_link.h"
#include "gesture.h"
#include "led.h"
#include "midi_map.h"
#include "param_store.h"
#include "pot.h"
#include "settings.h"
#include "spi_link.h"
#include "storage.h"
#include "tempo.h"
#include "timebase.h"

// The H7 answers the G0's first frame; this covers a few dropped ones.
#define BOOT_ECHO_WAIT_MS   50u

#define LED_FULL            255u

// Menus (plan §3.3). Presets and engines are browsed in pages of 16, one per small
// LED pattern.
#define PAGE_SIZE           16u
#define MENU_TIMEOUT_MS     30000u      // with no gesture at all, a menu aborts to RUN
#define COUNT_BLINK_MS      400u        // one blink of the bank or page count
#define RECALL_PULSE_MS     1200u
#define STORE_PULSE_MS      500u        // faster than RECALL, to tell them apart
#define ENGINE_PULSE_MS     1200u
#define CONFIRM_MS          400u        // favourite set: the small LEDs flash
#define CONFIRM_FLASH_MS    100u
#define LOADING_BLINK_MS    200u
#define ERROR_BLINK_MS      120u
#define ERROR_BLINKS        4u
#define OPTION_SHOW_MS      1000u       // a moved settings pot shows its option

typedef enum {
    CONTEXT_BOOT,
    CONTEXT_BYPASS,
    CONTEXT_RUN,
    CONTEXT_RECALL,
    CONTEXT_STORE,
    CONTEXT_ENGINE,
    CONTEXT_SETTINGS
} UiContext;

static UiContext context = CONTEXT_BOOT;
static uint32_t  bootDeadline;
static uint8_t   onLed;
static uint8_t   auxLed;

// Tempo LED. A blink starts where the beat phase wraps, and lasts a quarter beat.
static uint16_t  lastBeatPhase;
static uint32_t  blinkStartUs;
static bool      tapThisTick;

// Menu state. menuIndex is the slot in RECALL and STORE, the registry index in ENGINE.
static uint8_t   menuIndex;
static uint8_t   menuCount;
static uint32_t  menuDeadline;
static bool      countShowing;      // the bank or page count is blinking
static bool      countRestart;      // start it again on this tick's render
static bool      confirming;
static uint32_t  confirmStartMs;
static bool      errorShowing;
static bool      errorRestart;

// SETTINGS state. A pot edits its setting once it has moved on this page.
static uint8_t   settingsPage;
static uint8_t   potsTaken;
static bool      optionShowing;
static uint8_t   optionValue;
static uint32_t  optionStartMs;

_Static_assert(SETTINGS_POTS <= 8u, "potsTaken holds one bit per settings pot");

void uiInit(void)
{
    onLed  = boardPwmLedIndex(SWITCH_ROLE_ON);
    auxLed = boardPwmLedIndex(SWITCH_ROLE_AUX);
    bootDeadline = deadlineSet(BOOT_ECHO_WAIT_MS);
    paramStoreInit();
    settingsInit();
    tempoSetFollowMidi(settingsFollowMidiClock());
    midiSetChannel(settingsMidiChannel(), settingsMidiOmni());
}

static void showError(void)
{
    errorShowing = true;
    errorRestart = true;
}

// A slot that was never saved, or one saved with no engine, reads as empty.
static bool readPreset(uint8_t slot, Preset* preset)
{
    return storageRead(STORAGE_TYPE_PRESET, slot, preset, sizeof(*preset)) && preset->engineId != ENGINE_ID_NONE;
}

static void applyPreset(const Preset* preset, uint8_t slot)
{
    paramStoreApply(preset, slot);
    tempoRecall(preset->tempo, timebaseNowUs());
}

static void recall(uint8_t slot)
{
    Preset preset;
    if (readPreset(slot, &preset)) { applyPreset(&preset, slot); }
    else                           { paramStoreSetSlot(slot); }     // empty: only the index changes
}

// Cold boot, or a warm reset the H7 cannot restore: the favourite, bypassed. On a
// new unit the favourite slot is empty, and the G0 takes registry index 0 with its
// defaults.
static void enterFavourite(void)
{
    uint8_t favourite = 0u;
    if (!storageRead(STORAGE_TYPE_FAVOURITE, 0u, &favourite, sizeof(favourite)) ||
        favourite >= STORAGE_PRESET_SLOTS)
    {
        favourite = 0u;
    }

    Preset preset;
    if (readPreset(favourite, &preset))
    {
        applyPreset(&preset, favourite);
    }
    else
    {
        paramStoreSetSlot(favourite);
        paramStoreRequestEngineAt(0u);
    }
    potRebaseline();
    context = CONTEXT_BYPASS;
}

// Cold boot with ON held. The favourite is already loading, unengaged, so leaving
// is only a change of context.
static void enterSettings(void)
{
    settingsBeginEdit();
    context       = CONTEXT_SETTINGS;
    settingsPage  = 0u;
    potsTaken     = 0u;
    optionShowing = false;
    countRestart  = true;
}

// Warm reset: resume from the state the H7 last applied (plan §4.3).
static void adoptEcho(const H7ToUiPacket* echo)
{
    paramStoreRestore(echo);
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
    if (bootWasCold())
    {
        enterFavourite();
        if (bootWantsSettings()) { enterSettings(); }
        return;
    }

    const H7ToUiPacket* echo = spiLinkEcho();
    const uint16_t ready = H7_FLAG_STATE_VALID | H7_FLAG_ENGINE_READY;
    if (echo != NULL && (echo->h7Flags & H7_FLAG_LOADING) != 0u)
    {
        // A switch the player started before the reset. Its end is the state to
        // resume, however long the load takes.
        bootDeadline = deadlineSet(BOOT_ECHO_WAIT_MS);
    }
    else if (echo != NULL && (echo->h7Flags & ready) == ready && echo->activeEngine == echo->targetEngine)
    {
        adoptEcho(echo);
    }
    else if (deadlineExpired(bootDeadline))
    {
        enterFavourite();
    }
}

static void stepMode(int8_t direction)
{
    const uint8_t options = engineDescModeOptions(paramStoreDesc());
    if (!paramStoreReady() || options == 0u) { return; }
    const uint8_t value = paramStoreField(ENGINE_MODE_FIELD);
    // Clamped, so a held switch always lands at a known end.
    if (direction > 0 && value + 1u < options) { paramStoreSetField(ENGINE_MODE_FIELD, (uint8_t) (value + 1u)); }
    if (direction < 0 && value > 0u)           { paramStoreSetField(ENGINE_MODE_FIELD, (uint8_t) (value - 1u)); }
}

static void enterMenu(UiContext menu, uint8_t index, uint8_t count)
{
    context      = menu;
    menuIndex    = index;
    menuCount    = count;
    menuDeadline = deadlineSet(MENU_TIMEOUT_MS);
    countRestart = true;
    confirming   = false;
}

// Pots are ignored in the menus. Re-taking the baselines on the way out keeps a pot
// moved there from jumping its param.
static void leaveMenu(void)
{
    context = CONTEXT_RUN;
    potRebaseline();
}

static void enterEngineSelect(void)
{
    const H7ToUiPacket* echo = spiLinkEcho();
    if (echo == NULL || echo->engineCount == 0u || !paramStoreReady()) { return; }
    const uint8_t index = (echo->activeIndex < echo->engineCount) ? echo->activeIndex : 0u;
    enterMenu(CONTEXT_ENGINE, index, echo->engineCount);
}

static void handleRun(const Gesture* gesture)
{
    const uint32_t desc = paramStoreDesc();
    const uint8_t auxRole = engineDescAuxRole(desc);
    switch (gesture->type)
    {
        case GESTURE_ON_SHORT:
            context = CONTEXT_BYPASS;
            break;
        case GESTURE_ON_HOLD:
            enterMenu(CONTEXT_RECALL, paramStorePresetIndex(), STORAGE_PRESET_SLOTS);
            break;
        case GESTURE_BOTH_HOLD:
            enterMenu(CONTEXT_STORE, paramStorePresetIndex(), STORAGE_PRESET_SLOTS);
            break;
        case GESTURE_AUX_HOLD:
            enterEngineSelect();
            break;
        case GESTURE_AUX_SHORT:
            if (auxRole == AUX_ROLE_TAP)
            {
                tempoTap(gesture->pressUs);
                tapThisTick = true;
            }
            else if (auxRole == AUX_ROLE_TOGGLE)
            {
                const uint8_t field = engineDescAuxField(desc);
                paramStoreSetField(field, (uint8_t) (paramStoreField(field) ^ 1u));
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

// ±1 wraps over the whole list. A page step keeps the position within the page and
// clamps on a short last page.
static uint8_t stepIndex(uint8_t index, uint8_t count, int8_t direction, bool page)
{
    if (!page)
    {
        return (uint8_t) ((direction > 0) ? (index + 1u) % count : (index + count - 1u) % count);
    }
    const uint8_t pages = (uint8_t) ((count + PAGE_SIZE - 1u) / PAGE_SIZE);
    uint8_t p = (uint8_t) (index / PAGE_SIZE);
    p = (uint8_t) ((direction > 0) ? (p + 1u) % pages : (p + pages - 1u) % pages);
    const uint16_t next = (uint16_t) (p * PAGE_SIZE + index % PAGE_SIZE);
    return (next < count) ? (uint8_t) next : (uint8_t) (count - 1u);
}

static void navigate(int8_t direction, bool page)
{
    const uint8_t next = stepIndex(menuIndex, menuCount, direction, page);
    // A new page is shown by its count, and input during the count starts it again.
    if (countShowing || next / PAGE_SIZE != menuIndex / PAGE_SIZE) { countRestart = true; }
    menuIndex = next;
}

static void commit(void)
{
    switch (context)
    {
        case CONTEXT_RECALL:
            recall(menuIndex);
            break;
        case CONTEXT_ENGINE:
            // presetIndex stays, so a following STORE offers the slot the player came from.
            paramStoreRequestEngineAt(menuIndex);
            break;
        default:
            break;
    }
    leaveMenu();
}

static void store(void)
{
    Preset preset;
    paramStoreSnapshot(&preset);
    preset.tempo = engineDescUsesTempo(paramStoreDesc()) ? tempoInternalCentiBpm() : 0u;
    // Not while loading: the values would belong to the engine being left.
    if (paramStoreReady() && storageWrite(STORAGE_TYPE_PRESET, menuIndex, &preset, sizeof(preset)))
    {
        paramStoreSetSlot(menuIndex);
    }
    else
    {
        showError();
    }
    leaveMenu();
}

static void setFavourite(void)
{
    const uint8_t slot = menuIndex;
    if (storageWrite(STORAGE_TYPE_FAVOURITE, 0u, &slot, sizeof(slot)))
    {
        confirming     = true;
        confirmStartMs = timebaseNowMs();
    }
    else
    {
        showError();
    }
}

static void handleMenu(const Gesture* gesture)
{
    menuDeadline = deadlineSet(MENU_TIMEOUT_MS);
    switch (gesture->type)
    {
        case GESTURE_ON_SHORT:  navigate(1, false);  break;
        case GESTURE_AUX_SHORT: navigate(-1, false); break;
        case GESTURE_MODE_UP:   navigate(1, true);   break;
        case GESTURE_MODE_DOWN: navigate(-1, true);  break;
        case GESTURE_AUX_HOLD:  leaveMenu();         break;
        case GESTURE_ON_HOLD:
            if (context != CONTEXT_STORE) { commit(); }
            break;
        case GESTURE_BOTH_HOLD:
            if (context == CONTEXT_STORE)       { store(); }
            else if (context == CONTEXT_RECALL) { setFavourite(); }
            break;
        default:
            break;
    }
}

// Both ways out land in BYPASS with the favourite, as a cold boot without ON does.
static void leaveSettings(bool save)
{
    if (save)
    {
        if (!settingsSave()) { showError(); }
        tempoSetFollowMidi(settingsFollowMidiClock());
        midiSetChannel(settingsMidiChannel(), settingsMidiOmni());
    }
    context = CONTEXT_BYPASS;
    potRebaseline();
}

// Clamped, as the mode options are. A pot moved on the last page must not write the
// setting it has on this one, so the baselines are re-taken.
static void stepSettingsPage(int8_t direction)
{
    if (direction > 0 && settingsPage + 1u < SETTINGS_PAGES_USED) { settingsPage++; }
    else if (direction < 0 && settingsPage > 0u)                 { settingsPage--; }
    else                                                         { countRestart = true; return; }
    potsTaken     = 0u;
    optionShowing = false;
    countRestart  = true;
    potRebaseline();
}

static void handleSettings(const Gesture* gesture)
{
    switch (gesture->type)
    {
        case GESTURE_ON_HOLD:   leaveSettings(true);   break;
        case GESTURE_AUX_HOLD:  leaveSettings(false);  break;
        case GESTURE_MODE_UP:   stepSettingsPage(1);   break;
        case GESTURE_MODE_DOWN: stepSettingsPage(-1);  break;
        default:
            break;
    }
}

static void handleGesture(const Gesture* gesture)
{
    switch (context)
    {
        case CONTEXT_BYPASS:
            if (gesture->type == GESTURE_ON_SHORT) { context = CONTEXT_RUN; }
            break;
        case CONTEXT_RUN:
            handleRun(gesture);
            break;
        case CONTEXT_RECALL:
        case CONTEXT_STORE:
        case CONTEXT_ENGINE:
            handleMenu(gesture);
            break;
        case CONTEXT_SETTINGS:
            handleSettings(gesture);
            break;
        default:
            break;
    }
}

static bool inMenu(void)
{
    return context == CONTEXT_RECALL || context == CONTEXT_STORE || context == CONTEXT_ENGINE;
}

static void applyPots(void)
{
    for (uint8_t i = 0; i < kBoard.numPots; i++)
    {
        if (potMoved(i)) { paramStoreSetParam(kBoard.pots[i].paramIndex, potValue(i)); }
    }
}

// A pot that has moved writes its param on every tick, so a CC for that param holds
// only once the pot is back behind its takeover threshold.
static void releasePots(uint8_t param)
{
    for (uint8_t i = 0; i < kBoard.numPots; i++)
    {
        if (kBoard.pots[i].paramIndex == param) { potRebaselineOne(i); }
    }
}

// The engaged state stays. In a menu, the menu is left first, as an abort leaves it.
static void midiLoad(uint16_t program)
{
    if (program >= STORAGE_PRESET_SLOTS) { return; }
    if (inMenu()) { leaveMenu(); }
    recall((uint8_t) program);
    potRebaseline();
}

static void handleMidiAction(const MidiAction* action)
{
    switch (action->type)
    {
        case MIDI_ACTION_LOAD:
            midiLoad(action->value);
            break;
        case MIDI_ACTION_ENGAGE:
            if (context == CONTEXT_BYPASS && action->value != 0u)   { context = CONTEXT_RUN; }
            else if (context == CONTEXT_RUN && action->value == 0u) { context = CONTEXT_BYPASS; }
            break;
        case MIDI_ACTION_FIELD:
            // Ignored while the engine loads, as the mode switch is.
            paramStoreSetField(action->index, (uint8_t) action->value);
            break;
        case MIDI_ACTION_PARAM:
            paramStoreSetParam(action->index, action->value);
            releasePots(action->index);
            break;
        default:
            break;
    }
}

void uiMidi(const MidiMessage* message, uint32_t atUs)
{
    switch (message->status)
    {
        case MIDI_CLOCK:    tempoMidiClock(atUs);  return;
        case MIDI_START:    tempoMidiStart();      return;
        case MIDI_CONTINUE: tempoMidiContinue();   return;
        case MIDI_STOP:     tempoMidiStop();       return;
        case MIDI_SONG_POSITION:
            tempoMidiSongPosition((uint16_t) (message->data1 | ((uint16_t) message->data2 << 7)));
            return;
        default:
            break;
    }

    if (context == CONTEXT_BOOT || context == CONTEXT_SETTINGS) { return; }
    MidiAction action;
    if (midiMap(message, paramStoreDesc(), &action)) { handleMidiAction(&action); }
}

// A pot past the takeover threshold sets the option under it, in equal shares of its
// travel, and shows the option while it changes.
static void applySettingsPots(void)
{
    const uint8_t pots = (kBoard.numPots < SETTINGS_POTS) ? kBoard.numPots : SETTINGS_POTS;
    for (uint8_t i = 0; i < pots; i++)
    {
        const uint8_t options = settingsOptions(settingsPage, i);
        if (options == 0u || !potMoved(i)) { continue; }
        const uint8_t option = (uint8_t) (((uint32_t) potValue(i) * options) >> 16);
        const bool    taken  = (potsTaken & (1u << i)) != 0u;
        if (taken && option == settingsEditValue(settingsPage, i)) { continue; }

        potsTaken |= (uint8_t) (1u << i);
        settingsEditSet(settingsPage, i, option);
        // The channel bits show as the live channel instead.
        if (settingsPage == SETTINGS_PAGE_MIDI && i < SETTINGS_CHANNEL_BITS) { continue; }
        optionShowing = true;
        optionValue   = option;
        optionStartMs = timebaseNowMs();
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

// The error blink holds the ON LED until it ends. Setting another pattern meanwhile
// would restart both on every tick.
static void setOnLed(const LedPattern* pattern)
{
    if (!errorShowing) { ledPwmSet(onLed, pattern); }
}

// PWM LEDs in a menu: the count of the bank or page, then a pulse on both (RECALL,
// STORE) or on AUX alone (ENGINE).
static void renderMenuPwm(void)
{
    const LedPattern off = { LED_OFF, 0u, 0u, 0u };
    if (countRestart)
    {
        countRestart = false;
        countShowing = true;
        // Through off, so a count already playing starts again.
        setOnLed(&off);
        ledPwmSet(auxLed, &off);
    }
    if (countShowing && ledPwmDone(auxLed)) { countShowing = false; }

    if (countShowing)
    {
        const LedPattern count = { LED_BLINK_N, LED_FULL, COUNT_BLINK_MS, (uint8_t) (menuIndex / PAGE_SIZE + 1u) };
        setOnLed(&count);
        ledPwmSet(auxLed, &count);
        return;
    }

    const uint16_t periodMs = (context == CONTEXT_STORE)  ? STORE_PULSE_MS :
                              (context == CONTEXT_ENGINE) ? ENGINE_PULSE_MS : RECALL_PULSE_MS;
    const LedPattern pulse = { LED_PULSE, LED_FULL, periodMs, 0u };
    setOnLed((context == CONTEXT_ENGINE) ? &off : &pulse);
    ledPwmSet(auxLed, &pulse);
}

static void renderPerformancePwm(void)
{
    const LedPattern off     = { LED_OFF, 0u, 0u, 0u };
    const LedPattern on      = { LED_ON, LED_FULL, 0u, 0u };
    const LedPattern loading = { LED_BLINK, LED_FULL, LOADING_BLINK_MS, 0u };
    const bool run  = context == CONTEXT_RUN;
    const uint32_t desc = paramStoreDesc();

    // A toggle field shows its state in RUN. Otherwise the AUX LED blinks with the
    // tempo, in BYPASS too, whenever the engine uses one and one is set.
    const bool beat = beatLit();
    bool auxLit;
    if (run && engineDescAuxRole(desc) == AUX_ROLE_TOGGLE)
    {
        auxLit = paramStoreField(engineDescAuxField(desc)) != 0u;
    }
    else
    {
        auxLit = engineDescUsesTempo(desc) && beat;
    }
    ledPwmSet(auxLed, auxLit ? &on : &off);

    if (!paramStoreReady()) { setOnLed(&loading); }
    else                    { setOnLed(run ? &on : &off); }
}

// PWM LEDs in SETTINGS: ON solid, and AUX blinks the page number on entry and on each
// page step, since the MIDI page fills the small LEDs with the channel.
static void renderSettingsPwm(void)
{
    const LedPattern off = { LED_OFF, 0u, 0u, 0u };
    const LedPattern on  = { LED_ON, LED_FULL, 0u, 0u };
    setOnLed(&on);
    if (countRestart)
    {
        countRestart = false;
        countShowing = true;
        ledPwmSet(auxLed, &off);
    }
    if (countShowing && ledPwmDone(auxLed)) { countShowing = false; }

    const LedPattern count = { LED_BLINK_N, LED_FULL, COUNT_BLINK_MS, (uint8_t) (settingsPage + 1u) };
    ledPwmSet(auxLed, countShowing ? &count : &off);
}

static uint8_t smallLeds(void)
{
    if (confirming)
    {
        const uint32_t elapsed = timebaseNowMs() - confirmStartMs;
        if (elapsed < CONFIRM_MS) { return ((elapsed / CONFIRM_FLASH_MS) & 1u) ? 0x0u : 0xFu; }
        confirming = false;
    }
    if (context == CONTEXT_SETTINGS)
    {
        // One-hot, so option 0 still lights an LED.
        if (optionShowing && timebaseNowMs() - optionStartMs < OPTION_SHOW_MS) { return (uint8_t) (1u << optionValue); }
        optionShowing = false;
        if (settingsPage == SETTINGS_PAGE_MIDI) { return settingsEditMidiChannel(); }
        return (uint8_t) (1u << settingsPage);
    }
    if (inMenu())
    {
        // Slot 0 shows all off: the pulsing PWM LEDs already say which menu this is.
        return countShowing ? 0u : (uint8_t) (menuIndex % PAGE_SIZE);
    }
    const uint32_t desc = paramStoreDesc();
    if (context == CONTEXT_RUN && paramStoreReady() && engineDescModeOptions(desc) > 0u)
    {
        return (uint8_t) (1u << paramStoreField(ENGINE_MODE_FIELD));
    }
    return 0u;
}

static void renderLeds(void)
{
    if (inMenu())
    {
        (void) beatLit();   // keeps tracking the phase
        renderMenuPwm();
    }
    else if (context == CONTEXT_SETTINGS)
    {
        (void) beatLit();
        renderSettingsPwm();
    }
    else
    {
        renderPerformancePwm();
    }

    // The dimmer is for playing; the menus run at full brightness.
    const bool performing = context == CONTEXT_BYPASS || context == CONTEXT_RUN;
    ledSetDimmer(performing ? settingsDimmer() : LED_FULL);

    // The error blink overrides whatever the ON LED shows, until it ends.
    if (errorShowing)
    {
        const LedPattern off   = { LED_OFF, 0u, 0u, 0u };
        const LedPattern error = { LED_BLINK_N, LED_FULL, ERROR_BLINK_MS, ERROR_BLINKS };
        if (errorRestart)
        {
            errorRestart = false;
            ledPwmSet(onLed, &off);
        }
        ledPwmSet(onLed, &error);
        if (ledPwmDone(onLed)) { errorShowing = false; }
    }

    ledGpioSet(smallLeds());
}

void uiTick(void)
{
    if (context == CONTEXT_BOOT)
    {
        resolveBoot();
        if (context == CONTEXT_BOOT) { return; }
    }

    const uint8_t events = paramStoreTrack(spiLinkEcho());
    if ((events & (PARAM_STORE_ADOPTED | PARAM_STORE_REVERTED)) != 0u) { potRebaseline(); }
    if ((events & PARAM_STORE_REVERTED) != 0u) { showError(); }

    Gesture gesture;
    const uint32_t nowUs = timebaseNowUs();
    while (gestureNext(nowUs, &gesture)) { handleGesture(&gesture); }

    if (inMenu() && deadlineExpired(menuDeadline)) { leaveMenu(); }
    if (context == CONTEXT_SETTINGS) { applySettingsPots(); }
    else if (!inMenu())              { applyPots(); }
    renderLeds();
}

bool uiFillFrame(UiToH7Packet* tx)
{
    if (context == CONTEXT_BOOT) { return false; }

    // Engaged in RUN and in the menus entered from it: browsing leaves the audio alone.
    const bool engaged = context == CONTEXT_RUN || inMenu();
    const uint32_t period = tempoPeriodUs();
    tx->runFlags     = (uint16_t) (settingsRunFlags() | (engaged ? RUN_FLAG_ENGAGED : 0u) |
                                   (tempoSource() << RUN_FLAG_TEMPO_SRC_SHIFT));
    paramStoreFill(tx);
    tx->eventToggles = 0u;
    tx->tempoHz      = (period != 0u) ? 1000000.0f / (float) period : 0.0f;
    tx->tempoPhase   = (period != 0u) ? tempoPhaseAt(spiLinkNextEdgeUs()) : 0u;
    return true;
}
