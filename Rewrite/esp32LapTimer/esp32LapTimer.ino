
#define TotalLanes 3
#define Timer_Render_Delay 10 // 10 milisecond

/*
1000 milisecond = 1 second
1000 ms == 1 s
1 ms = 0.001 s
10 ms = 0.01 s
*/

// 3 Lane Limit Swich
volatile uint8_t laneIndex[TotalLanes] = { 0, 1, 2 };
volatile uint8_t limitSwitchPins[TotalLanes] = { 12, 13, 14 };
// 3 Lane LED
volatile uint8_t ledLane[TotalLanes] = { 5, 6, 7};

volatile uint8_t readyButtonPin = 3;
volatile uint8_t finalLapButtonPin = 4;

enum States {
	Ready,
	Started,
	FinalLap,
	Finished
};

volatile States currentState = States::Ready;
volatile uint32_t startTime;
volatile uint32_t timer;
volatile uint32_t last_timer;

void ready() {
	currentState = States::Ready;
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		uint8_t idx = laneIndex[i];
		digitalWrite(ledLane[i], LOW); // turn off led
		attachInterruptArg(limitSwitchPins[i], &LimitSwitch_ISR, &idx, FALLING); // enable limit switch trigger
	}
	last_timer = 0;
	timer = 0;
	renderTimer();
}
void started(uint8_t laneIdx) {
	for (int8_t i = 0; i < TotalLanes; i++)
		detachInterrupt(limitSwitchPins[i]); // disable limit switch trigger
	startTime = millis(); // init current timestamp
	currentState = States::Started;
}
void finalLap() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		uint8_t idx = laneIndex[i];
		attachInterruptArg(limitSwitchPins[i], &LimitSwitch_ISR, &idx, FALLING); // enable limit switch trigger
		digitalWrite(ledLane[i], HIGH); // turn on led
	}
	currentState = States::FinalLap;
}
void finished(uint8_t laneIdx) {
	currentState = States::Finished;
	for (int8_t i = 0; i < TotalLanes; i++) 
	{
		detachInterrupt(limitSwitchPins[i]); // disable limit switch trigger
		if (laneIdx != i)
			digitalWrite(ledLane[i], LOW);  // turn off led, except finished lane
	}
}

// Limit Switch Trigger
void IRAM_ATTR LimitSwitch_ISR(void *arg) {
	uint8_t* ptr = static_cast<uint8_t*>(arg);
	uint8_t laneIdx = *ptr;
	
	switch (currentState)
	{
		case States::Ready:
			started(laneIdx);
			break;
		case States::FinalLap:
			finished(laneIdx);
			break;
		default:
			return;
	}
}

// Ready Button Trigger
void IRAM_ATTR ReadyButton_ISR() {
	if (currentState != States::FinalLap && currentState != States::Finished)
		return;
	ready();
}

// Final Lap Button Trigger
void IRAM_ATTR FinalLapButton_ISR() {
	if (currentState != States::Started)
		return;
	finalLap();
}

// logic Render Timer Display
void renderTimer() {
	
}

void setup() {
	for (int8_t i = 0; i < TotalLanes; i++)
	{
		pinMode(limitSwitchPins[i], INPUT_PULLUP);
		pinMode(ledLane[i], OUTPUT);
	}
	attachInterrupt(readyButtonPin, &ReadyButton_ISR, FALLING); // enable ready button trigger
	attachInterrupt(finalLapButtonPin, &FinalLapButton_ISR, FALLING); // enable final lap button trigger
	ready();
}

void loop() {
	if (currentState != States::Started)
		return;

	timer = millis() - startTime;
	if (timer - last_timer >= Timer_Render_Delay)
	{
		last_timer = timer;
		renderTimer();
	}
}