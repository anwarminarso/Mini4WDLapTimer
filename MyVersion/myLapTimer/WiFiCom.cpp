//------------------------------------------------------------------
// Copyright(c) 2022 Anwar Minarso (anwar.minarso@gmail.com)
// https://github.com/anwarminarso/
// This file is part of the ESP32+ PLC.
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

#include <WiFi.h>
#include <ESPmDNS.h>
#include "WifiCom.h"
#include "GlobalVariables.h"
#include "Configuration.h"

TimerHandle_t tmrWiFiHealth;
bool IsWiFiSTAMode = false;

static void tmrWiFiHealthFun(xTimerHandle pxTimer) {

	if (WiFi.status() == WL_CONNECTED) {
		deviceState.ReconnectCount = 0;
		deviceState.ResetCount = 0;
	}
	else {
		if (deviceState.ReconnectCount > 20) {
			deviceState.ReconnectCount = 0;
			deviceState.ResetCount = 0;
			updateDeviceState();
			ESP.restart();
		}
		else {
			deviceState.ReconnectCount++;
			Serial.println("Reconnecting to WiFi...");
			WiFi.reconnect();
		}
	}
}


void initWiFi() {
	int connectionAttempt = 0;
	IsWiFiSTAMode = false;
	WiFi.persistent(false);
	WiFi.disconnect(true, true);
	delay(100);
	String hostName;
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
	delay(500);
	if (config.WiFi_STA_Enabled) {
		hostName = String(config.WiFi_STA_HostName);
		hostName.trim();
		char* pHostName = &hostName[0];
		if (!WiFi.setHostname(pHostName))
		{
			Serial.print("Failure to set hostname. Current Hostname : ");
			Serial.println(WiFi.getHostname());
		}
		WiFi.mode(WIFI_STA);
		
		WiFi.begin(config.WiFi_STA_SSID, config.WiFi_STA_Password);
		Serial.print("Connecting to WiFi ..");
		while (WiFi.status() != WL_CONNECTED) {
			Serial.print('.');
			connectionAttempt++;
			if (connectionAttempt >= 31) {
				if (deviceState.ResetCount > 3) {
					deviceState.ReconnectCount = 0;
					deviceState.ResetCount = 0;
					updateDeviceState();
					break;
				}
				deviceState.ReconnectCount = 0;
				deviceState.ResetCount++;
				updateDeviceState();
				ESP.restart();
			}
			delay(1000);
		}
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println();
		IsWiFiSTAMode = false;
		if (config.WiFi_STA_Enabled)
			Serial.println("Unable to connect WiFi !!!");
		WiFi.persistent(false);
		WiFi.disconnect(true, true);
		WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
		delay(500);
		hostName = String(config.WiFi_AP_HostName);
		hostName.trim();
		WiFi.softAPsetHostname(hostName.c_str());
		WiFi.mode(WIFI_AP);
		WiFi.softAP(config.WiFi_AP_SSID, config.WiFi_AP_Password);
		Serial.print("Starting to WiFi AP Mode.. Host IP: ");
		Serial.println(WiFi.softAPIP());
	}
	else {
		IsWiFiSTAMode = true;
		deviceState.ReconnectCount = 0;
		deviceState.ResetCount = 0;
		Serial.println(WiFi.localIP());
	}
	if (MDNS.begin(hostName.c_str())) {
		MDNS.addService("http", "tcp", 80);
	}
	else
	{
		Serial.println("Error setting up MDNS responder!");
	}
	if (IsWiFiSTAMode) {
		tmrWiFiHealth = xTimerCreate(
			"tmrWiFiHealth", /* name */
			pdMS_TO_TICKS(3000), /* period/time */
			pdTRUE, /* auto reload */
			(void*)1, /* timer ID */
			&tmrWiFiHealthFun); /* callback */
		if (xTimerStart(tmrWiFiHealth, 10) != pdPASS) {
			Serial.println("Timer start error");
		}
	}
}
void resetWiFi() {
	if (IsWiFiSTAMode) {
		xTimerDelete(tmrWiFiHealth, 0);
	}
	deviceState.ReconnectCount = 0;
	deviceState.ResetCount = 0;
	WiFi.disconnect();
	initWiFi();
}
