#include <Arduino.h>
#include "GlobalVariables.h"
#include "Configuration.h"
#include "WiFiCom.h"
#include "WebSvr.h"
#include "RealTimeClock.h"
#include "ClientAPI.h"
#include "LapTimer.h"


void setup() {
	Serial.begin(115200);
	delay(10);
	//resetConfig();
	loadConfig();
	initWiFi();
	initRTC();
	initWebServer();
	initClientAPI();

	initLapTimer();
}
void loop() {
	loopNTP();
	clientAPILoop();
	loopLapTimer();
}