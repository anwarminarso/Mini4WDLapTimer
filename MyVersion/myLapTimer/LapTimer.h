
//------------------------------------------------------------------
// Copyright(c) 2023 Anwar Minarso (anwar.minarso@gmail.com)
// https://github.com/anwarminarso/
// This file is part of esp32LapTimer project
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
// Lesser General Public License for more details
//------------------------------------------------------------------
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