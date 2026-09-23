#include "switch.h"

// Majority of the last 5 samples.
#define HISTORY_MASK   0x1Fu
#define MAJORITY       3u

// A 1 ms tick produces at most PRESS, or SHORT and RELEASE, or HOLD/REPEAT per switch.
#define QUEUE_SIZE     16u

typedef struct {
    uint8_t  history;
    bool     down;
    bool     locked;        // down at boot, ignored until released
    bool     holdSent;
    uint32_t pressUs;
    uint32_t nextUs;        // time of the next HOLD or REPEAT
} SwitchState;

static SwitchState state[SWITCH_MAX];
static uint8_t     switchCount;
static uint32_t    repeats;

static SwitchEvent queue[QUEUE_SIZE];
static uint8_t     queueHead;
static uint8_t     queueCount;

static void push(uint8_t index, SwitchEventType type)
{
    // Full only if the thread stopped reading for several ticks; the newest is dropped.
    if (queueCount == QUEUE_SIZE) { return; }
    SwitchEvent* event = &queue[(queueHead + queueCount) % QUEUE_SIZE];
    event->index   = index;
    event->type    = type;
    event->pressUs = state[index].pressUs;
    queueCount++;
}

static uint8_t bitCount(uint8_t bits)
{
    uint8_t count = 0;
    for (; bits != 0u; bits &= (uint8_t) (bits - 1u)) { count++; }
    return count;
}

void switchInit(uint8_t count, uint32_t repeatMask, uint32_t downMask)
{
    switchCount = (count > SWITCH_MAX) ? SWITCH_MAX : count;
    repeats = repeatMask;
    queueHead = 0;
    queueCount = 0;
    for (uint8_t i = 0; i < switchCount; i++)
    {
        const bool down = (downMask & (1u << i)) != 0u;
        state[i] = (SwitchState) { down ? HISTORY_MASK : 0u, down, down, false, 0u, 0u };
    }
}

void switchPoll(uint32_t downMask, uint32_t nowUs)
{
    for (uint8_t i = 0; i < switchCount; i++)
    {
        SwitchState* s = &state[i];
        s->history = (uint8_t) (((s->history << 1) | ((downMask >> i) & 1u)) & HISTORY_MASK);
        const bool down = bitCount(s->history) >= MAJORITY;

        if (down != s->down)
        {
            s->down = down;
            if (s->locked)
            {
                if (!down) { s->locked = false; }
            }
            else if (down)
            {
                s->pressUs  = nowUs;
                s->nextUs   = nowUs + SWITCH_HOLD_US;
                s->holdSent = false;
                push(i, SWITCH_EVENT_PRESS);
            }
            else
            {
                if (!s->holdSent) { push(i, SWITCH_EVENT_SHORT); }
                push(i, SWITCH_EVENT_RELEASE);
            }
            continue;
        }

        if (!s->down || s->locked) { continue; }
        if ((int32_t) (nowUs - s->nextUs) < 0) { continue; }
        if (!s->holdSent)
        {
            s->holdSent = true;
            push(i, SWITCH_EVENT_HOLD);
        }
        else if ((repeats & (1u << i)) != 0u)
        {
            push(i, SWITCH_EVENT_REPEAT);
        }
        else
        {
            continue;
        }
        s->nextUs = s->nextUs + SWITCH_REPEAT_US;
    }
}

bool switchNextEvent(SwitchEvent* event)
{
    if (queueCount == 0u) { return false; }
    *event = queue[queueHead];
    queueHead = (uint8_t) ((queueHead + 1u) % QUEUE_SIZE);
    queueCount--;
    return true;
}
