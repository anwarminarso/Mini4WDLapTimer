//------------------------------------------------------------------
 // Copyright(c) 2023 a2n Technology
 // Anwar Minarso (anwar.minarso@gmail.com)
 // https://github.com/anwarminarso/
 // This file is part of the Mini4WDLapTimer project.
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

#include <ShiftRegister74HC595.h>

#define TotalLanes 3


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

uint8_t segmentCodes[Timer_Digit] = { 0, 0, 0, 0 };
ShiftRegister74HC595<Timer_Digit> sr(dataPin, clockPin, latchPin);

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


/*
32 Ready Button
33 Final Lap Button
25 Limit Switch Lane A
26 Limit Switch Lane B
27 Limit Switch Lane C
14 LED Lane A
12 LED Lane B
13 LED Lane C
26 LED A
25 LED B
33 LED C
32 LS A
35 LS B
34 LS C
39 Ready
36 Final Lap / Reset
*/


//readyButton, finalButton, Lane A, Lane B, Lane C
uint8_t inputPins[2 + TotalLanes] = { 32, 33, 25, 26, 27 };
uint8_t ledLane[TotalLanes] = { 14, 12, 13 };

// 3 Lane Limit Swich
uint8_t laneIndex[TotalLanes] = { 0, 1, 2 };

enum States {
	Ready,
	Started,
	FinalLap,
	Finished
};

States currentState = States::Ready;
uint32_t startTime;
uint32_t timer;
uint32_t timer_delay;
uint32_t last_timer_delay;
bool flipflop = false;

void ready() {
	currentState = States::Ready;
	for (int8_t i = 0; i < TotalLanes + 2; i++)
	{
		pinMode(inputPins[0], INPUT_PULLUP);
	}
	timer_delay = 0;
	last_timer_delay = 0;
	timer = 0;
	renderTimer();
	//Serial.println("Ready");
}
void started(uint8_t laneIdx) {
	startTime = millis(); // init current timestamp
	currentState = States::Started;
	//Serial.println("Started");
}
void cancelFinalLap() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		digitalWrite(ledLane[i], LOW); // turn on led
	}
	currentState = States::Started;
}
void finalLap() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		digitalWrite(ledLane[i], HIGH); // turn on led
	}
	currentState = States::FinalLap;
	//Serial.println("Final Lap");
}
void finished(uint8_t laneIdx) {
	currentState = States::Finished;
	for (int8_t i = 0; i < TotalLanes; i++) 
	{
		if (laneIdx != i)
			digitalWrite(ledLane[i], LOW);  // turn off led, except finished lane
	}
	//Serial.println("Finished");
}

// logic Render Timer Display
void renderTimer() {
	flipflop = ((timer / 100) % 10) < 5;
	for (uint8_t i = 0; i < Timer_Digit; i++)
	{
		uint32_t div = pow10(i) * 10;
		uint8_t digitNumber = (timer / div) % 10;
		uint8_t invIdx = Timer_Digit - i - 1;

		if (i != 1)
		{
			segmentCodes[invIdx] = numbersCode[digitNumber];
			if (i == 2 && flipflop)
				segmentCodes[invIdx] &= ~(1 << 7);
		}
		else
		{
			segmentCodes[invIdx] = inverseNumbersCode[digitNumber];
			if (flipflop)
				segmentCodes[invIdx] &= ~(1 << 6);
		}
	}
	sr.setAll(segmentCodes);
}

void renderFinishedTimer() {
	segmentCodes[1] &= ~(1 << 6);
	segmentCodes[2] &= ~(1 << 7);
	sr.setAll(segmentCodes);
}
void setup() {
	//Serial.begin(115200);

	for (int8_t i = 0; i < sizeof(inputPins); i++)
	{
		pinMode(inputPins[i], INPUT_PULLUP);
	}
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		pinMode(ledLane[i], OUTPUT);
	}
	ready();
}

void loop() {
	
	switch (currentState)
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
							finished(i - 2);
							break;
						}
					}
				}
			}
			break;
		case States::Finished:
			{
				if (digitalRead(inputPins[0]) == LOW)
					ready();
			}
			break;
	}
	if (currentState == States::Started || currentState == States::FinalLap) {
		timer = millis() - startTime;
		timer_delay = timer / Timer_Render_Delay;
		if (timer_delay - last_timer_delay > 0)
		{
			last_timer_delay = timer_delay;
			renderTimer();
		}
	}
}