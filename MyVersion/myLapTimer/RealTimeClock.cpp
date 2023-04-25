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

#include "RealTimeClock.h"
#include "GlobalVariables.h"
#include <WiFi.h>
#include <RTClib.h>
#include "Utils.h"
#include "ESPNtpClient.h"
WiFiUDP ntpUDP;

RTC_DS3231 rtc;
bool IsRTCAvaliable;
uint32_t offsetEpochTime;
uint32_t offsetEpochMillis;

bool syncEventTriggered = false; // True if a time even has been triggered
NTPEvent_t ntpEvent; // Last triggered event

void processSyncEvent(NTPEvent_t ntpEvent) {
    Serial.printf("[NTP-event] %s\n", NTP.ntpEvent2str(ntpEvent));
    if (ntpEvent.event == 0 || ntpEvent.event == 3) {
        while (true)
        {
            long deviceMillis = millis();
            long ntpMillis = NTP.millis();
            long utcTime = ntpMillis;
            if (utcTime < 0)
                utcTime *= -1;
            if (utcTime % 1000 == 0) {
                time_t currentTime = time(NULL);
                long epochTime = currentTime + config.NTP_Time_Offset;
                DateTime dt = DateTime(epochTime);
                rtc.adjust(dt);
                Serial.println("NTP Millis: " + String(ntpMillis));
                Serial.println("NTP Time: " + String(utcTime));
                Serial.println("Epoch Time: " + String(epochTime));
                Serial.println("Current Time: " + String(currentTime));
                Serial.println("Device Time: " + String(deviceMillis));
                break;
            }
        }
        synchRTC();
        NTP.stop();
    }
}


void initRTC() {
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        IsRTCAvaliable = false;
    }
    else {
        IsRTCAvaliable = true;
    }
    if (IsRTCAvaliable) {
        NTP.onNTPSyncEvent([](NTPEvent_t event) {
            ntpEvent = event;
            syncEventTriggered = true;
        });
        NTP.setInterval(300);
        
        if (rtc.lostPower())
            updateRTC();
    }
}

uint32_t getCurrentTime() {
    if (IsRTCAvaliable) {
        DateTime dt = rtc.now();
        return dt.unixtime();
    }
    else {
        DateTime dt = DateTime(__DATE__, __TIME__);
        return dt.unixtime();
    }
}
void loopNTP() {
    if (!IsRTCAvaliable)
        return;
	if (syncEventTriggered) {
        syncEventTriggered = false;
		processSyncEvent(ntpEvent);
	}
}
void synchRTC() {
    if (!IsRTCAvaliable)
        return;
    DateTime startDT = rtc.now();
    
    uint32_t startRTC = startDT.unixtime();
    uint32_t endMillis = 0;
    uint32_t endRTC = startRTC;
    while (true)
    {
        DateTime dt = rtc.now();
        endRTC = dt.unixtime();
        if (startRTC != endRTC) {
            endMillis = millis();
            break;
        }
    }
    offsetEpochTime = endRTC;
    offsetEpochMillis = endMillis;
    Serial.print("Unix RTC :");
    Serial.println(endRTC);
}
void updateRTC() {
    if (!IsRTCAvaliable)
        return;
    String serverName = trimChar(config.NTP_Server_Address);
    NTP.stop();
    NTP.setTimeZone(TZ_Etc_Universal);
    NTP.begin(serverName.c_str());
    
    /*if (NTP.begin(serverName.c_str())) {
        while (true)
        {
            if (syncEventTriggered) {
				syncEventTriggered = false;
				processSyncEvent(ntpEvent);
                if (ntpEvent.event == 0 || ntpEvent.event == 3) {
                    break;
                }
            }
        }
        while (true)
        {
            long utcTime = NTP.millis();
            if (utcTime % 1000 == 0) {
                long epochTime = (utcTime / 1000L) + config.NTP_Time_Offset;
                DateTime dt = DateTime(epochTime);
                rtc.adjust(dt);
                Serial.println("UTC Time: " + String(utcTime));
                Serial.println("NTP Epoch Time: " + String(epochTime));
                break;
            }
        }
        synchRTC();
        NTP.stop();
    }*/
}