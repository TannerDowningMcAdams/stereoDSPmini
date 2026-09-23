#include "timebase.h"
#include "tim.h"

// TIM14 counts microseconds (1 MHz) and wraps every PERIOD_US. The period is a whole
// number of ms, so both clocks stay exact modulo 2^32. It is also longer than a flash
// page erase stall (~40 ms), so at most one overflow is ever pending.
#define PERIOD_US 64000u
#define PERIOD_MS (PERIOD_US / 1000u)

// Written only by the TIM14 ISR, which runs at priority 0 so no reader can preempt it
// between clearing UIF and counting the period.
static volatile uint32_t periods = 0;

void timebaseInit(void)
{
    HAL_TIM_Base_Start_IT(&htim14);
}

static void sample(uint32_t* periodsOut, uint32_t* countOut)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t p = periods;
    uint32_t count = TIM14->CNT;
    // An overflow not yet counted by the ISR. The counter is re-read, since the
    // first read may predate the wrap.
    if ((TIM14->SR & TIM_SR_UIF) != 0u)
    {
        p = p + 1u;
        count = TIM14->CNT;
    }
    __set_PRIMASK(primask);
    *periodsOut = p;
    *countOut = count;
}

uint32_t timebaseNowUs(void)
{
    uint32_t p, count;
    sample(&p, &count);
    return p * PERIOD_US + count;
}

uint32_t timebaseNowMs(void)
{
    uint32_t p, count;
    sample(&p, &count);
    return p * PERIOD_MS + count / 1000u;
}

uint32_t deadlineSet(uint32_t fromNowMs)
{
    return timebaseNowMs() + fromNowMs;
}

bool deadlineExpired(uint32_t deadline)
{
    return (int32_t) (timebaseNowMs() - deadline) >= 0;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM14) { periods = periods + 1u; }
}
