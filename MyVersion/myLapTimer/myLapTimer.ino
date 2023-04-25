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