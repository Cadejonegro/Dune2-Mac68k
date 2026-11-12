/*
 * Stage 10G: Macintosh System 7 timing adapter.
 *
 * OpenDUNE normally runs its timer at 60 Hz.
 * Classic Macintosh TickCount runs at approximately 60 Hz.
 */

#include <Events.h>

#include "../../OpenDUNE/include/types.h"
#include "../../OpenDUNE/src/timer.h"

volatile uint32 g_timerGUI = 0;
volatile uint32 g_timerGame = 0;
volatile uint32 g_timerInput = 0;
volatile uint32 g_timerSleep = 0;
volatile uint32 g_timerTimeout = 0;

static uint16 s_timersActive = 0;
static uint32 s_lastMacTick = 0;

/* Stage 15O-L.2 timer diagnostics. */
volatile uint32 g_macTimerCapHits = 0;
volatile uint32 g_macTimerDiscardedTicks = 0;
volatile uint32 g_macTimerMaximumElapsed = 0;


bool Timer_SetTimer(TimerType timer, bool set)
{
    uint16 mask = (uint16)(1 << (timer - 1));
    bool previous = (s_timersActive & mask) != 0;

    if (set)
        s_timersActive |= mask;
    else
        s_timersActive &= (uint16)~mask;

    return previous;
}

void Timer_Tick(void)
{
    if (s_timersActive & 1)
        g_timerGUI++;

    if (s_timersActive & 2)
        g_timerGame++;

    g_timerInput++;
    g_timerSleep++;

    if (g_timerTimeout != 0)
        g_timerTimeout--;
}

/*
 * Called regularly by the Macintosh event loop.
 * One OpenDUNE tick per classic Macintosh tick.
 */
void MacTimer_Update(void)
{
    uint32 now = TickCount();
    uint32 elapsed;

    if (s_lastMacTick == 0) {
        s_lastMacTick = now;
        return;
    }

    elapsed = now - s_lastMacTick;

    if (elapsed > g_macTimerMaximumElapsed)
        g_macTimerMaximumElapsed = elapsed;

    /* R3D.7-A: timer cap experiment. Preserve all other behavior.
     * Permit up to 60 elapsed ticks; longer stalls remain capped.
     */
    if (elapsed > 60) {
        g_macTimerCapHits++;
        g_macTimerDiscardedTicks += elapsed - 60;
        elapsed = 60;
    }

    while (elapsed-- > 0) {
        Timer_Tick();
    }

    s_lastMacTick = now;
}

void SleepAndProcessBackgroundTasks(void)
{
    MacTimer_Update();
}

void Timer_Sleep(uint16 ticks)
{
    uint32 start = g_timerSleep;

    while ((uint32)(g_timerSleep - start) < ticks)
        MacTimer_Update();
}
