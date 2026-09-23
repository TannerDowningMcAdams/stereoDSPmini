#include "led.h"
#include "board.h"
#include "main.h"
#include "timebase.h"

// Perceptual brightness correction for 8-bit PWM (ARR = 255)
static const uint8_t gamma8[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };
typedef struct {
    LedPattern pattern;
    uint32_t   startMs;
    uint8_t    from;         // brightness when the pattern started, for RAMP
    uint8_t    brightness;   // before gamma and dimmer
} PwmLed;

static PwmLed  pwm[BOARD_MAX_PWM_LEDS];
static uint8_t dimmer = 255u;

static volatile uint32_t* ccr(const BoardPwmLed* led)
{
    // CCR1..CCR4 are consecutive registers.
    return &led->tim->CCR1 + (led->channel - 1u);
}

void ledInit(void)
{
    for (uint8_t i = 0; i < kBoard.numPwmLeds; i++)
    {
        const BoardPwmLed* led = &kBoard.pwmLeds[i];
        *ccr(led) = 0u;
        led->tim->CCER |= TIM_CCER_CC1E << (4u * (led->channel - 1u));
        led->tim->BDTR |= TIM_BDTR_MOE;
        led->tim->CR1  |= TIM_CR1_CEN;
    }
    ledGpioSet(0u);
}

static bool samePattern(const LedPattern* a, const LedPattern* b)
{
    return a->mode == b->mode && a->level == b->level &&
           a->periodMs == b->periodMs && a->count == b->count;
}

void ledPwmSet(uint8_t led, const LedPattern* pattern)
{
    if (led >= kBoard.numPwmLeds || samePattern(&pwm[led].pattern, pattern)) { return; }
    pwm[led].pattern = *pattern;
    pwm[led].from    = pwm[led].brightness;
    pwm[led].startMs = timebaseNowMs();
}

bool ledPwmDone(uint8_t led)
{
    if (led >= kBoard.numPwmLeds) { return true; }
    const LedPattern* p = &pwm[led].pattern;
    return p->mode == LED_BLINK_N &&
           (timebaseNowMs() - pwm[led].startMs) >= (uint32_t) p->periodMs * p->count;
}

void ledSetDimmer(uint8_t scale)
{
    dimmer = scale;
}

void ledGpioSet(uint8_t mask)
{
    for (uint8_t i = 0; i < kBoard.numGpioLeds; i++)
    {
        const BoardGpioLed* led = &kBoard.gpioLeds[i];
        HAL_GPIO_WritePin(led->port, led->pin, ((mask >> i) & 1u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

static uint8_t square(uint32_t elapsed, uint16_t period, uint8_t level)
{
    return (period == 0u || (elapsed % period) < period / 2u) ? level : 0u;
}

static uint8_t brightness(const PwmLed* led, uint32_t elapsed)
{
    const LedPattern* p = &led->pattern;
    switch (p->mode)
    {
        case LED_ON:
            return p->level;
        case LED_BLINK:
            return square(elapsed, p->periodMs, p->level);
        case LED_RAMP:
            if (elapsed >= p->periodMs) { return p->level; }
            return (uint8_t) (led->from + ((int32_t) p->level - led->from) * (int32_t) elapsed / p->periodMs);
        case LED_PULSE:
        {
            if (p->periodMs < 2u) { return p->level; }
            const uint32_t half  = p->periodMs / 2u;
            const uint32_t phase = elapsed % p->periodMs;
            const uint32_t tri   = (phase < half) ? phase : p->periodMs - phase;
            return (uint8_t) ((p->level * (tri > half ? half : tri)) / half);
        }
        case LED_BLINK_N:
            if (elapsed >= (uint32_t) p->periodMs * p->count) { return 0u; }
            return square(elapsed, p->periodMs, p->level);
        case LED_OFF:
        default:
            return 0u;
    }
}

void ledRender(void)
{
    const uint32_t now = timebaseNowMs();
    for (uint8_t i = 0; i < kBoard.numPwmLeds; i++)
    {
        pwm[i].brightness = brightness(&pwm[i], now - pwm[i].startMs);
        *ccr(&kBoard.pwmLeds[i]) = ((uint32_t) gamma8[pwm[i].brightness] * dimmer) / 255u;
    }
}
