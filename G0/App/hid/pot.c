#include "pot.h"

// In 12-bit LSBs. The 256x oversampled scan is quiet, so a small band suffices.
#define HYSTERESIS   6u
#define TAKEOVER     64u     // ~1.6% of travel
#define END_ZONE     16u
#define SPAN         (4095u - 2u * END_ZONE)

typedef struct {
    uint16_t held;          // hysteresis output
    uint16_t baseline;
    bool     valid;
    bool     moved;
} PotState;

static PotState pots[POT_MAX];
static uint8_t  potCount;

static uint16_t distance(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t) (a - b) : (uint16_t) (b - a);
}

void potInit(uint8_t count)
{
    potCount = (count > POT_MAX) ? POT_MAX : count;
    for (uint8_t i = 0; i < potCount; i++) { pots[i] = (PotState) { 0u, 0u, false, false }; }
}

void potUpdate(uint8_t index, uint16_t raw)
{
    if (index >= potCount) { return; }
    PotState* pot = &pots[index];
    if (!pot->valid)
    {
        *pot = (PotState) { raw, raw, true, false };
        return;
    }
    if (distance(raw, pot->held) > HYSTERESIS) { pot->held = raw; }
    if (!pot->moved && distance(pot->held, pot->baseline) > TAKEOVER) { pot->moved = true; }
}

void potRebaseline(void)
{
    for (uint8_t i = 0; i < potCount; i++)
    {
        pots[i].baseline = pots[i].held;
        pots[i].moved = false;
    }
}

bool potMoved(uint8_t index)
{
    return index < potCount && pots[index].moved;
}

uint16_t potValue(uint8_t index)
{
    if (index >= potCount) { return 0u; }
    const uint16_t held = pots[index].held;
    if (held <= END_ZONE) { return 0u; }
    if (held >= 4095u - END_ZONE) { return 65535u; }
    return (uint16_t) (((uint32_t) (held - END_ZONE) * 65535u) / SPAN);
}
