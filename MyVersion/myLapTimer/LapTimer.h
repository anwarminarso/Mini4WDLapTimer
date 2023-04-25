
#ifndef _LAP_TIMER_H_
#define _LAP_TIMER_H_
void initLapTimer();
void loopLapTimer();

void resetRaceState(bool copyToLast);
void updateRacerLap(uint8_t laneIdx);
void renderTimer();
void renderFinishedTimer();
void loopLapTimerManual();
void loopLapTimerAuto();
#endif