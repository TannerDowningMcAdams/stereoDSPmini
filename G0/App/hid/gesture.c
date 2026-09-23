#include "gesture.h"
#include "switch.h"

enum { FOOT_ON, FOOT_AUX, FOOT_COUNT };

static uint8_t footIndex[FOOT_COUNT];
static uint8_t upIndex;
static uint8_t downIndex;

// One press cycle runs from the first footswitch going down until both are up.
static bool     footDown[FOOT_COUNT];
static bool     chord;          // both were down together at some point this cycle
static bool     chordSettled;   // BOTH_HOLD fired, or the chord was cancelled
static bool     singleHold;     // a single HOLD fired this cycle
static uint32_t chordUs;        // second press of the chord

void gestureInit(uint8_t on, uint8_t aux, uint8_t up, uint8_t down)
{
    footIndex[FOOT_ON]  = on;
    footIndex[FOOT_AUX] = aux;
    upIndex   = up;
    downIndex = down;
}

static bool emit(Gesture* gesture, GestureType type, uint32_t pressUs)
{
    gesture->type    = type;
    gesture->pressUs = pressUs;
    return true;
}

// Returns true and fills gesture if the event produces one.
static bool footEvent(uint8_t foot, const SwitchEvent* event, Gesture* gesture)
{
    const uint8_t other = (uint8_t) (FOOT_COUNT - 1u - foot);
    switch (event->type)
    {
        case SWITCH_EVENT_PRESS:
            footDown[foot] = true;
            if (footDown[other] && !chord)
            {
                chord = true;
                chordUs = event->pressUs;
                // A hold already delivered keeps the cycle; the chord cannot fire.
                chordSettled = singleHold;
            }
            return false;

        case SWITCH_EVENT_SHORT:
            if (chord) { return false; }
            return emit(gesture, (foot == FOOT_ON) ? GESTURE_ON_SHORT : GESTURE_AUX_SHORT, event->pressUs);

        case SWITCH_EVENT_HOLD:
            if (chord) { return false; }
            singleHold = true;
            return emit(gesture, (foot == FOOT_ON) ? GESTURE_ON_HOLD : GESTURE_AUX_HOLD, event->pressUs);

        case SWITCH_EVENT_RELEASE:
            footDown[foot] = false;
            chordSettled = true;
            if (!footDown[other])
            {
                chord = false;
                chordSettled = false;
                singleHold = false;
            }
            return false;

        default:
            return false;
    }
}

bool gestureNext(uint32_t nowUs, Gesture* gesture)
{
    if (chord && !chordSettled && (int32_t) (nowUs - (chordUs + SWITCH_HOLD_US)) >= 0)
    {
        chordSettled = true;
        return emit(gesture, GESTURE_BOTH_HOLD, chordUs);
    }

    SwitchEvent event;
    while (switchNextEvent(&event))
    {
        if (event.index == footIndex[FOOT_ON] && footEvent(FOOT_ON, &event, gesture))   { return true; }
        if (event.index == footIndex[FOOT_AUX] && footEvent(FOOT_AUX, &event, gesture)) { return true; }

        const bool step = event.type == SWITCH_EVENT_SHORT || event.type == SWITCH_EVENT_HOLD ||
                          event.type == SWITCH_EVENT_REPEAT;
        if (step && event.index == upIndex)   { return emit(gesture, GESTURE_MODE_UP, event.pressUs); }
        if (step && event.index == downIndex) { return emit(gesture, GESTURE_MODE_DOWN, event.pressUs); }
    }
    return false;
}
