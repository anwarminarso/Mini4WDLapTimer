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

#include "Arduino.h"
#ifndef _TYPEDEFSTRUCT_H_
#define _TYPEDEFSTRUCT_H_

typedef struct {
    char WiFi_STA_HostName[32];
    char WiFi_STA_SSID[32];
    char WiFi_STA_Password[64];
    uint8_t WiFi_STA_Enabled;

    char WiFi_AP_HostName[32];
    char WiFi_AP_SSID[32];
    char WiFi_AP_Password[64];


    char Admin_UserName[32];
    char Admin_Password[32];

    char MQTT_Server_Address[100];
    uint16_t MQTT_Server_Port;
    char MQTT_Root_Topic[100];
    char MQTT_Server_UserName[32];
    char MQTT_Server_Password[32];
    uint16_t MQTT_Tele_Interval;
    uint8_t MQTT_Enabled;

    char NTP_Server_Address[100];
    int NTP_Time_Offset;
    int RTC_Time_Offset;
    
    uint8_t LapTimer_TotalInterChange;
    uint8_t LapTimer_TotalLap;
    uint8_t LapTimer_AutoModeEnabled;
    uint32_t LapTimer_StartDelay;

    uint8_t LapTimer_Auto_BTO;
    uint32_t LapTimer_BTO;
    uint8_t LapTimer_Auto_Limit;
    uint32_t LapTimer_Delta_Limit;
    uint32_t LapTimer_Limit;
    
} memConfig_t;



enum States {
    Ready,
    Started,
    FinalLap,
    Finished
};

typedef struct {
    uint8_t currentLane;
    uint8_t nextLane;
    long lapTime;
} lapTime_t;

typedef struct {
    uint8_t currentLap;
    lapTime_t lapTimes[10];
    long finishTime;
    bool isAvailable;
    bool isDisqualified;
} racerState_t;


typedef struct {
    States currentState;
    long startTime;
    long currentTime;
    uint8_t currentLap;
    racerState_t racersState[3];
} raceState_t;


typedef struct {
    uint8_t ResetCount;
    uint8_t ReconnectCount;
} deviceState_t;
#endif