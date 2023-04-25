
#include "GlobalVariables.h"
#include "LapTimer.h"
#include "ClientAPI.h"
#include "WebSvr.h"
#include <ShiftRegister74HC595.h>
#include "Queue.h"
#include "RealTimeClock.h"
#include "WebSvr.h"
#include "ClientAPI.h"
#include "RaceState.h"
#include "Configuration.h"

#define TotalLanes 3


// Use Inverse Number on 3rd Digit
// Comment this if not use inverse number on 3rd digit
//#define USE_INVERSE_NUMBER_3  

/*
1000 milisecond = 1 second
1000 ms == 1 s
1 ms = 0.001 s
10 ms = 0.01 s
*/
#define Timer_Render_Delay 10 // 10 milisecond

#define Timer_Digit 4 // 4 digit



//Pin connected to latch pin (ST_CP) of 74HC595
const int latchPin = 5;
//Pin connected to clock pin (SH_CP) of 74HC595
const int clockPin = 18;
//Pin connected to Data in (DS) of 74HC595
const int dataPin = 19;

Queue<uint8_t> laneQueue[TotalLanes] = { Queue<uint8_t>(TotalLanes + 1), Queue<uint8_t>(TotalLanes + 1) , Queue<uint8_t>(TotalLanes + 1) };

uint8_t segmentCodes[Timer_Digit] = { 0, 0, 0, 0 };
ShiftRegister74HC595<Timer_Digit> sr(dataPin, clockPin, latchPin);
bool buttonRacerAvailable[TotalLanes];

raceState_t raceState;
raceState_t lastRaceState;

uint8_t numbersCode[10] = {
	B11000000, //0
	B11111001, //1
	B10100100, //2
	B10110000, //3
	B10011001, //4
	B10010010, //5
	B10000010, //6
	B11111000, //7
	B10000000, //8
	B10010000 //9
};
uint8_t inverseNumbersCode[10] = {
	B11000000, //0
	B11111001, //1
	B01100100, //2
	B01110000, //3
	B01011001, //4
	B01010010, //5
	B01000010, //6
	B11111000, //7
	B01000000, //8
	B01010000 //9
};

//readyButton, finalButton, Lane A, Lane B, Lane C
uint8_t inputPins[2 + TotalLanes] = { 32, 33, 25, 26, 27 };
uint8_t ledLane[TotalLanes] = { 14, 12, 13 };

// 3 Lane Limit Swich
uint8_t laneIndex[TotalLanes] = { 0, 1, 2 };



uint32_t timer_delay;
uint32_t last_timer_delay;
bool flipflop = false;
bool isRacerSlotOpen = false;

uint8_t currentRacerCount = 0;



void ready(bool copyResult) {
	resetRaceState(copyResult);
	for (int8_t i = 0; i < TotalLanes + 2; i++)
	{
		digitalWrite(ledLane[i], LOW); // turn on led
	}
	timer_delay = 0;
	last_timer_delay = 0;
	renderTimer();
	sendLapTimerStatus();
	sendLapTimerBTO();
	sendMqttLapTimerStatus();
	Serial.println("Ready");
}
void cancelFinalLap() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		digitalWrite(ledLane[i], LOW); // turn on led
	}
	raceState.currentState = States::Started;
}
void finalLap() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		digitalWrite(ledLane[i], HIGH); // turn on led
	}
	if (raceState.currentState < States::FinalLap) {
		raceState.currentState = States::FinalLap;
		sendLapTimerStatus();
		sendMqttLapTimerStatus();
	}
	//Serial.println("Final Lap");
}
void started(uint8_t laneIdx) {
	if (raceState.currentState != States::Started) {
		raceState.startTime = millis(); // init current timestamp
		raceState.currentState = States::Started;
		sendLapTimerStatus();
		sendMqttLapTimerStatus();
	}
	updateRacerLap(laneIdx);
	//Serial.println("Started");
}
uint8_t getNextLane(uint8_t laneIdx) {
	uint8_t n = laneIdx + config.LapTimer_TotalInterChange + 1;
	if (n <= TotalLanes)
		return n - 1;
	else {
		uint8_t m = n % TotalLanes;
		if (m == 0)
			return n - 1;
		else
			return m - 1;
	}
}
void updateRacerLap(uint8_t laneIdx) {
	if (!buttonRacerAvailable[laneIdx])
		return;
	buttonRacerAvailable[laneIdx] = false;
	if (isRacerSlotOpen) {
		if (raceState.racersState[laneIdx].isAvailable)
			return;

		raceState.racersState[laneIdx].isAvailable = true;
		raceState.racersState[laneIdx].currentLap = 1;

		uint8_t nextLane = raceState.racersState[laneIdx].lapTimes[raceState.racersState[laneIdx].currentLap - 1].nextLane;
		laneQueue[nextLane].push(laneIdx);
		if (raceState.currentLap < raceState.racersState[laneIdx].currentLap)
			raceState.currentLap = raceState.racersState[laneIdx].currentLap;
		currentRacerCount++;
		sendRacerStatus(laneIdx, -1);
		sendMqttRacerStatus(laneIdx, -1);
	}
	else {
		uint8_t racerIdx = 0;
		bool isValid = false;
		while (laneQueue[laneIdx].count() > 0)
		{
			racerIdx = laneQueue[laneIdx].pop();
			if (raceState.racersState[racerIdx].isDisqualified) {
				continue;
			}
			else {
				isValid = true;
				break;
			}
		}
		if (!isValid)
			return;
		//racerState_t * racer = &raceState.racersState[racerIdx];
		raceState.racersState[racerIdx].lapTimes[raceState.racersState[racerIdx].currentLap - 1].lapTime = millis() - raceState.startTime;
		if (raceState.racersState[racerIdx].currentLap == config.LapTimer_TotalLap) {
			raceState.racersState[racerIdx].finishTime = raceState.racersState[racerIdx].lapTimes[raceState.racersState[racerIdx].currentLap - 1].lapTime;
			raceState.racersState[racerIdx].currentLap++;
			if (raceState.racersState[racerIdx].finishTime > config.LapTimer_Limit)
				raceState.racersState[racerIdx].isDisqualified = 1;
			currentRacerCount--;
			if (currentRacerCount == 0) {
				raceState.currentTime = raceState.racersState[racerIdx].finishTime;
				finished(laneIdx, true);
				renderFinishedTimer();
			}
			else {
				finished(laneIdx, false);
			}
			
			sendRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
			sendMqttRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
		}
		else {
			raceState.racersState[racerIdx].currentLap++;
			uint8_t nextLane = raceState.racersState[racerIdx].lapTimes[raceState.racersState[racerIdx].currentLap - 1].nextLane;
			//laneQueue[raceState.racersState[laneIdx].nextLane].push(racer);
			laneQueue[nextLane].push(racerIdx);
			if (raceState.currentLap < raceState.racersState[racerIdx].currentLap)
				raceState.currentLap = raceState.racersState[racerIdx].currentLap;
			if (raceState.racersState[racerIdx].currentLap == config.LapTimer_TotalLap) {
				finalLap();
			}
			sendRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
			sendMqttRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
		}


	}
}
void finished(uint8_t laneIdx, bool sendState = true) {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		if (laneIdx != i)
			digitalWrite(ledLane[i], LOW);  // turn off led, except finished lane
		else
			digitalWrite(ledLane[i], HIGH);  // turn off led, except finished lane
	}
	if (raceState.currentState < States::Finished) {
		raceState.currentState = States::Finished;
	}
	if (sendState) {
		sendLapTimerStatus();
		sendMqttLapTimerStatus();
	}
}

// logic Render Timer Display
void renderTimer() {
	flipflop = ((raceState.currentTime / 100) % 10) < 5;
	for (uint8_t i = 0; i < Timer_Digit; i++)
	{
		uint32_t div = pow10(i) * 10;
		uint8_t digitNumber = (raceState.currentTime / div) % 10;
		uint8_t invIdx = Timer_Digit - i - 1;
#ifdef USE_INVERSE_NUMBER_3
		if (i != 1) {
			segmentCodes[invIdx] = numbersCode[digitNumber];
			if (i == 2 && flipflop)
				segmentCodes[invIdx] &= ~(1 << 7);
		}
		else {
			segmentCodes[invIdx] = inverseNumbersCode[digitNumber];
			if (flipflop)
				segmentCodes[invIdx] &= ~(1 << 6);
	}
#else
		segmentCodes[invIdx] = numbersCode[digitNumber];
		if (i == 2 && flipflop)
			segmentCodes[invIdx] &= ~(1 << 7);
#endif
}
	sr.setAll(segmentCodes);
}

void renderFinishedTimer() {
#ifdef USE_INVERSE_NUMBER_3
	segmentCodes[2] &= ~(1 << 6);
	segmentCodes[1] &= ~(1 << 7);
#else
	segmentCodes[1] &= ~(1 << 7);
#endif
	sr.setAll(segmentCodes);
}

void resetRaceState(bool copyToLast) {
	if (copyToLast) {
		lastRaceState.startTime = raceState.startTime;
		lastRaceState.currentTime = raceState.currentTime;
		lastRaceState.currentLap = raceState.currentLap;
		for (uint8_t i = 0; i < TotalLanes; i++)
		{
			lastRaceState.racersState[i].isAvailable = raceState.racersState[i].isAvailable;
			lastRaceState.racersState[i].currentLap = raceState.racersState[i].currentLap;
			lastRaceState.racersState[i].finishTime = raceState.racersState[i].finishTime;
			lastRaceState.racersState[i].isDisqualified = raceState.racersState[i].isDisqualified;
			if (config.LapTimer_Auto_BTO
				&& lastRaceState.racersState[i].isAvailable
				&& !lastRaceState.racersState[i].isDisqualified
				&& lastRaceState.racersState[i].finishTime > 0
				&& lastRaceState.racersState[i].finishTime < config.LapTimer_BTO) {
				config.LapTimer_BTO = lastRaceState.racersState[i].finishTime;
				if (config.LapTimer_Auto_Limit) {
					config.LapTimer_Limit = config.LapTimer_BTO + config.LapTimer_Delta_Limit;
				}
				saveConfig();
			}
			for (uint8_t j = 0; j < 10; j++)
			{
				lastRaceState.racersState[i].lapTimes[j].lapTime = raceState.racersState[i].lapTimes[j].lapTime;
				lastRaceState.racersState[i].lapTimes[j].currentLane = raceState.racersState[i].lapTimes[j].currentLane;
				lastRaceState.racersState[i].lapTimes[j].nextLane = raceState.racersState[i].lapTimes[j].nextLane;
			}
		}
	}
	raceState.currentState = States::Ready;
	raceState.startTime = 0;
	raceState.currentTime = 0;
	raceState.currentLap = 0;
	for (uint8_t i = 0; i < TotalLanes; i++)
	{
		buttonRacerAvailable[i] = true;
		raceState.racersState[i].isAvailable = false;
		raceState.racersState[i].isDisqualified = false;
		raceState.racersState[i].finishTime = 0;
		raceState.racersState[i].currentLap = 0;
		for (uint8_t j = 0; j < config.LapTimer_TotalLap; j++)
		{
			if (j == 0)
				raceState.racersState[i].lapTimes[j].currentLane = i;
			else
				raceState.racersState[i].lapTimes[j].currentLane = raceState.racersState[i].lapTimes[j - 1].nextLane;
			raceState.racersState[i].lapTimes[j].nextLane = getNextLane(raceState.racersState[i].lapTimes[j].currentLane);
			raceState.racersState[i].lapTimes[j].lapTime = 0;
		}
		laneQueue[i].clear();
	}
	isRacerSlotOpen = true;
	currentRacerCount = 0;
}
void initLapTimer() {
	for (int8_t i = 0; i < sizeof(inputPins); i++)
	{
		pinMode(inputPins[i], INPUT_PULLUP);
	}
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		pinMode(ledLane[i], OUTPUT);
	}

	synchRTC();
	ready(false);
}


void disqualify(uint8_t racerIdx) {
	if (!config.LapTimer_AutoModeEnabled || racerIdx >= TotalLanes || raceState.currentState == States::Ready)
		return;
	if (!raceState.racersState[racerIdx].isAvailable || raceState.racersState[racerIdx].isDisqualified)
		return;
	raceState.racersState[racerIdx].isDisqualified = true;
	uint8_t nextLane = raceState.racersState[racerIdx].lapTimes[raceState.racersState[racerIdx].currentLap - 1].nextLane;
	if (laneQueue[nextLane].peek() == racerIdx)
		laneQueue[nextLane].pop();

	sendRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
	sendMqttRacerStatus(racerIdx, raceState.racersState[racerIdx].currentLap - 2);
	currentRacerCount--;
	if (currentRacerCount == 0) {
		finished(nextLane, true);
		renderFinishedTimer();
	}
}
void loopLapTimerManual() {
	switch (raceState.currentState)
	{
		case States::Ready:
			{
				for (uint8_t i = 2; i < sizeof(inputPins); i++)
				{
					if (digitalRead(inputPins[i]) == LOW)
					{
						started(i - 2);
						currentRacerCount = 1;
						break;
					}
				}
			}
			break;
		case States::Started:
			{
				if (digitalRead(inputPins[1]) == LOW)
					finalLap();
			}
			break;
		case States::FinalLap:
			{
				if (digitalRead(inputPins[0]) == LOW)
				{
					cancelFinalLap();
				}
				else
				{
					for (uint8_t i = 2; i < sizeof(inputPins); i++)
					{
						if (digitalRead(inputPins[i]) == LOW)
						{
							finished(i - 2, true);
							currentRacerCount = 0;
							renderFinishedTimer();
							break;
						}
					}
				}
			}
			break;
		case States::Finished:
			{
				if (digitalRead(inputPins[0]) == LOW)
					ready(true);
			}
			break;
	}
}
void loopLapTimerAuto() {

	switch (raceState.currentState)
	{
	case States::Ready:
	{
		for (uint8_t i = 2; i < sizeof(inputPins); i++)
		{
			if (digitalRead(inputPins[i]) == LOW)
			{
				started(i - 2);
				break;
			}
		}
	}
	break;
	case States::Started:
	{
		if (isRacerSlotOpen) {
			isRacerSlotOpen = (millis() - raceState.startTime) <= config.LapTimer_StartDelay;
		}
		for (uint8_t i = 2; i < sizeof(inputPins); i++)
		{
			if (digitalRead(inputPins[i]) == LOW)
			{
				updateRacerLap(i - 2);
				break;
			}
			else
				buttonRacerAvailable[i - 2] = true;
		}
	}
	break;
	case States::FinalLap:
	{
		for (uint8_t i = 2; i < sizeof(inputPins); i++)
		{
			if (digitalRead(inputPins[i]) == LOW)
			{
				updateRacerLap(i - 2);
				break;
			}
			else
				buttonRacerAvailable[i - 2] = true;
		}
	}
	break;
	case States::Finished:
	{
		if (digitalRead(inputPins[0]) == LOW)
			ready(true);
		else {
			for (uint8_t i = 2; i < sizeof(inputPins); i++)
			{
				if (digitalRead(inputPins[i]) == LOW)
					updateRacerLap(i - 2);
				else
					buttonRacerAvailable[i - 2] = true;
			}
		}
	}
	break;
	}
}
void loopLapTimer() {

	if (config.LapTimer_AutoModeEnabled) {
		loopLapTimerAuto();
	}
	else {
		loopLapTimerManual();
	}
	if (currentRacerCount > 0) {
		raceState.currentTime = millis() - raceState.startTime;
		timer_delay = raceState.currentTime / Timer_Render_Delay;
		if (timer_delay - last_timer_delay > 0)
		{
			last_timer_delay = timer_delay;
			renderTimer();
		}
	}
}