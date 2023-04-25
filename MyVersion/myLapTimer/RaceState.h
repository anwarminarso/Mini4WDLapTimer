
#ifndef _RACE_STATE_H_
#define _RACE_STATE_H_
void ready(bool copyResult);
void finalLap();
void cancelFinalLap();
void started(uint8_t laneIdx);
void disqualify(uint8_t racerIdx);
void finished(uint8_t laneIdx, bool sendState);
#endif 