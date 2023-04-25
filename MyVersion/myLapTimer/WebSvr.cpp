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
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebAuthentication.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "defines.h"
#include "GlobalVariables.h"
#include "WebSvr.h"
#include "Utils.h"
#include "Configuration.h"
#include "RaceState.h"
#include "RealTimeClock.h"

#ifdef  OTA_ENABLED
#include <Update.h>
#endif

#define DEBUG_WEB

AsyncWebServer webServer(80);
AsyncWebSocket ws("/ws");

#define MSG_BUFFER_SIZE 255
static uint8_t msgCode;
static uint8_t msgBuff[MSG_BUFFER_SIZE];
static uint8_t msgBuffIndex;
static bool IsUploading = false;

String uname;
String pwd;


TimerHandle_t tmrWSHealth;

static void tmrWSHealthFun(xTimerHandle pxTimer) {
	ws.cleanupClients();
}

void setWebSocketHeader(uint8_t cmd) {
	msgBuffIndex = 0;
	msgBuff[msgBuffIndex++] = cmd;
}
void setWebSocketContent(uint8_t* data, size_t len) {
	int i = len;
	while (i--)
		msgBuff[msgBuffIndex++] = *data++;
}
void sendWebSocketMessage(AsyncWebSocketClient* client) {
	client->binary(msgBuff, msgBuffIndex);
}
void sendAllWebSocketMessage() {
	ws.binaryAll(msgBuff, msgBuffIndex);
}
void executeWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
	msgCode = data[0];
	switch (msgCode)
	{
		case 101: // Ready
			if (config.LapTimer_AutoModeEnabled && raceState.currentState == States::Finished) {
				ready(true);
			}
			else {
				if (raceState.currentState == States::FinalLap)
					cancelFinalLap();
				else if (raceState.currentState == States::Finished)
					ready(true);
			}
			break;
		case 102: // Final Lap
			if (!config.LapTimer_AutoModeEnabled && raceState.currentState == States::Started) {
				finalLap();
			}
			break;
		case 104: // Disqualify
			disqualify(data[1]);
			break;
	}
}
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
	if (type == WS_EVT_CONNECT) {
		Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
		client->ping();
	}
	else if (type == WS_EVT_DISCONNECT) {
		Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
	}
	else if (type == WS_EVT_ERROR) {
		Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
	}
	else if (type == WS_EVT_PONG) {
		Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
	}
	else if (type == WS_EVT_DATA) {
		AwsFrameInfo* info = (AwsFrameInfo*)arg;
		String msg = "";
		if (info->final && info->index == 0 && info->len == len) {
			if (info->opcode != WS_TEXT) {
				if (info->len > 0) {
					if (IsUploading)
						return;
					executeWebSocketMessage(client, data, len);
				}
			}
		}
	}
}
void handleUploadFirmware(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
	
#ifndef DEBUG_WEB
	if (!request->authenticate(uname.c_str(), pwd.c_str()))
		return request->requestAuthentication();
#endif
	IsUploading = true;
	if (!index) {
		Serial.println("Updating...");
		//content_len = request->contentLength();
		//int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : ;
		
		if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
			Update.printError(Serial);
		}
	}
	if (!Update.hasError()) {
		if (Update.write(data, len) != len) {
			Update.printError(Serial);
		}
	}
	if (final) {
		AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
		response->addHeader("Refresh", "20");
		response->addHeader("Location", "/");
		request->send(response);
		if (!Update.end(true)) {
			Update.printError(Serial);
			IsUploading = false;
		}
		else {
			Serial.println("Update complete");
			Serial.flush();
			ESP.restart();
		}
	}
}

void initWebServer() {
	if (!SPIFFS.begin(true)) {
		return;
	}
	uname = String(config.Admin_UserName);
	pwd = String(config.Admin_Password);
	uname.trim();
	pwd.trim();
#ifdef DEBUG_WEB
	webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=6000");
#else
	webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=6000").setAuthentication(uname.c_str(), pwd.c_str());
#endif
	webServer.on("/logout.html", HTTP_GET, [](AsyncWebServerRequest* request) {
		request->send(200, "text/html", "<html><head><title>Logged Out</title></head><body><h1>You have been logged out...</h1></body></html>");
	});
	webServer.onNotFound([](AsyncWebServerRequest* request) {
		request->send(404, "text/plain", "The content you are looking for was not found.");
	});

	// REST API
	webServer.on("/api/logout", HTTP_GET, [](AsyncWebServerRequest* request) {
		request->send(401);
	});

	webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		DynamicJsonDocument resultDoc(1024);
		String resultJsonValue;
		resultDoc["Admin_UserName"] = trimChar(config.Admin_UserName);
		//resultDoc["Admin_Password"] = trimChar(config.Admin_Password);

		resultDoc["WiFi_AP_HostName"] = trimChar(config.WiFi_AP_HostName);
		resultDoc["WiFi_AP_SSID"] = trimChar(config.WiFi_AP_SSID);
		resultDoc["WiFi_AP_Password"] = trimChar(config.WiFi_AP_Password);

		resultDoc["WiFi_STA_Enabled"] = config.WiFi_STA_Enabled;
		resultDoc["WiFi_STA_HostName"] = trimChar(config.WiFi_STA_HostName);
		resultDoc["WiFi_STA_SSID"] = trimChar(config.WiFi_STA_SSID);
		resultDoc["WiFi_STA_Password"] = trimChar(config.WiFi_STA_Password);

		resultDoc["WiFi_MAC_Address"] = WiFi.macAddress();
		resultDoc["WiFi_IP_Address"] = WiFi.getMode() == WIFI_MODE_STA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();


		resultDoc["MQTT_Server_Address"] = trimChar(config.MQTT_Server_Address);
		resultDoc["MQTT_Server_Port"] = config.MQTT_Server_Port;
		resultDoc["MQTT_Root_Topic"] = trimChar(config.MQTT_Root_Topic);
		resultDoc["MQTT_Server_UserName"] = trimChar(config.MQTT_Server_UserName);
		resultDoc["MQTT_Server_Password"] = trimChar(config.MQTT_Server_Password);
		resultDoc["MQTT_Tele_Interval"] = config.MQTT_Tele_Interval;
		resultDoc["MQTT_Enabled"] = config.MQTT_Enabled;
		

		resultDoc["LapTimer_AutoModeEnabled"] = config.LapTimer_AutoModeEnabled;
		resultDoc["LapTimer_TotalLap"] = config.LapTimer_TotalLap;
		resultDoc["LapTimer_TotalInterChange"] = config.LapTimer_TotalInterChange;
		resultDoc["LapTimer_StartDelay"] = config.LapTimer_StartDelay;

		resultDoc["LapTimer_Auto_BTO"] = config.LapTimer_Auto_BTO;
		resultDoc["LapTimer_Auto_Limit"] = config.LapTimer_Auto_Limit;
		resultDoc["LapTimer_BTO"] = config.LapTimer_BTO;
		resultDoc["LapTimer_Limit"] = config.LapTimer_Limit;
		resultDoc["LapTimer_Delta_Limit"] = config.LapTimer_Delta_Limit;
		
		resultDoc["NTP_Server_Address"] = config.NTP_Server_Address;
		resultDoc["NTP_Time_Offset"] = config.NTP_Time_Offset;
		resultDoc["RTC_Time_Offset"] = config.RTC_Time_Offset;
		
#ifdef OTA_ENABLED
		resultDoc["IsOTAEnabled"] = 1;
#else
		resultDoc["IsOTAEnabled"] = 0;
#endif



		serializeJson(resultDoc, resultJsonValue);
#ifdef DEBUG_WEB
		AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resultJsonValue);
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
#else
		request->send(200, "application/json", resultJsonValue);
#endif
	});
	webServer.on("/api/update/security", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		String oldPassword;
		String newPassword;
		String comparePassword = String(config.Admin_Password);
		comparePassword.trim();
		if (request->hasParam("Admin_Old_Password", true)) {
			oldPassword = request->getParam("Admin_Old_Password", true)->value();
			oldPassword.trim();
		}
		if (request->hasParam("Admin_New_Password", true)) {
			newPassword = request->getParam("Admin_New_Password", true)->value();
			newPassword.trim();
		}
		if (oldPassword.equals(comparePassword)) {
			if (request->hasParam("Admin_UserName", true)) {
				String val = request->getParam("Admin_UserName", true)->value();
				strncpy((char*)&config.Admin_UserName, val.c_str(), 32);
			}
			strncpy((char*)&config.Admin_Password, newPassword.c_str(), 32);
			saveConfig();
			request->send(200);
		}
		else {
			request->send(400, "application/json", "Invalid password");
		}
	});
	webServer.on("/api/update/wifi", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		String val;
		if (request->hasParam("WiFi_AP_HostName", true)) {
			val = request->getParam("WiFi_AP_HostName", true)->value();
			strncpy((char*)&config.WiFi_AP_HostName, val.c_str(), 32);
		}
		if (request->hasParam("WiFi_AP_SSID", true)) {
			val = request->getParam("WiFi_AP_SSID", true)->value();
			strncpy((char*)&config.WiFi_AP_SSID, val.c_str(), 32);
		}
		if (request->hasParam("WiFi_AP_Password", true)) {
			val = request->getParam("WiFi_AP_Password", true)->value();
			strncpy((char*)&config.WiFi_AP_Password, val.c_str(), 64);
		}
		if (request->hasParam("WiFi_STA_Enabled", true)) {
			val = request->getParam("WiFi_STA_Enabled", true)->value();
			config.WiFi_STA_Enabled = val.toInt();
		}
		else
			config.WiFi_STA_Enabled = 0;
		if (request->hasParam("WiFi_STA_HostName", true)) {
			val = request->getParam("WiFi_STA_HostName", true)->value();
			strncpy((char*)&config.WiFi_STA_HostName, val.c_str(), 32);
		}
		if (request->hasParam("WiFi_STA_SSID", true)) {
			val = request->getParam("WiFi_STA_SSID", true)->value();
			strncpy((char*)&config.WiFi_STA_SSID, val.c_str(), 32);
		}
		if (request->hasParam("WiFi_STA_Password", true)) {
			val = request->getParam("WiFi_STA_Password", true)->value();
			strncpy((char*)&config.WiFi_STA_Password, val.c_str(), 64);
		}
		saveConfig();
		request->send(200);

	});
	webServer.on("/api/update/mqtt", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		String val;
		if (request->hasParam("MQTT_Server_Address", true)) {
			val = request->getParam("MQTT_Server_Address", true)->value();
			strncpy((char*)&config.MQTT_Server_Address, val.c_str(), 100);
		}
		if (request->hasParam("MQTT_Root_Topic", true)) {
			val = request->getParam("MQTT_Root_Topic", true)->value();
			strncpy((char*)&config.MQTT_Root_Topic, val.c_str(), 100);
		}
		if (request->hasParam("MQTT_Server_UserName", true)) {
			val = request->getParam("MQTT_Server_UserName", true)->value();
			strncpy((char*)&config.MQTT_Server_UserName, val.c_str(), 32);
		}
		if (request->hasParam("MQTT_Server_Password", true)) {
			val = request->getParam("MQTT_Server_Password", true)->value();
			strncpy((char*)&config.MQTT_Server_Password, val.c_str(), 32);
		}
		if (request->hasParam("MQTT_Server_Port", true)) {
			val = request->getParam("MQTT_Server_Port", true)->value();
			config.MQTT_Server_Port = val.toInt();
		}
		if (request->hasParam("MQTT_Tele_Interval", true)) {
			val = request->getParam("MQTT_Tele_Interval", true)->value();
			config.MQTT_Tele_Interval = val.toInt();
		}
		if (request->hasParam("MQTT_Enabled", true)) {
			val = request->getParam("MQTT_Enabled", true)->value();
			config.MQTT_Enabled = val.toInt();
		}
		else
			config.MQTT_Enabled = 0;
		saveConfig();
		request->send(200);
	});
	webServer.on("/api/update/lapTimer", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
		return request->requestAuthentication();
#endif
		String val;
		if (request->hasParam("LapTimer_AutoModeEnabled", true)) {
			val = request->getParam("LapTimer_AutoModeEnabled", true)->value();
			config.LapTimer_AutoModeEnabled = val.toInt();
		}
		else
			config.LapTimer_AutoModeEnabled = 0;
		if (request->hasParam("LapTimer_Auto_BTO", true)) {
			val = request->getParam("LapTimer_Auto_BTO", true)->value();
			config.LapTimer_Auto_BTO = val.toInt();
		}
		else
			config.LapTimer_Auto_BTO = 0;
		if (request->hasParam("LapTimer_Auto_Limit", true)) {
			val = request->getParam("LapTimer_Auto_Limit", true)->value();
			config.LapTimer_Auto_Limit = val.toInt();
		}
		else
			config.LapTimer_Auto_Limit = 0;
		if (request->hasParam("LapTimer_TotalInterChange", true)) {
			val = request->getParam("LapTimer_TotalInterChange", true)->value();
			config.LapTimer_TotalInterChange = val.toInt();
		}
		if (request->hasParam("LapTimer_StartDelay", true)) {
			val = request->getParam("LapTimer_StartDelay", true)->value();
			config.LapTimer_StartDelay = val.toInt();
		}
		if (request->hasParam("LapTimer_TotalLap", true)) {
			val = request->getParam("LapTimer_TotalLap", true)->value();
			config.LapTimer_TotalLap = val.toInt();
		}
		if (request->hasParam("LapTimer_BTO", true)) {
			val = request->getParam("LapTimer_BTO", true)->value();
			config.LapTimer_BTO = val.toInt();
		}
		if (request->hasParam("LapTimer_Limit", true)) {
			val = request->getParam("LapTimer_Limit", true)->value();
			config.LapTimer_Limit = val.toInt();
		}
		if (request->hasParam("LapTimer_Delta_Limit", true)) {
			val = request->getParam("LapTimer_Delta_Limit", true)->value();
			config.LapTimer_Delta_Limit = val.toInt();
		}
		saveConfig();
		request->send(200);
	});

	webServer.on("/api/update/ntp", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
		return request->requestAuthentication();
#endif
		String val;
		if (request->hasParam("NTP_Server_Address", true)) {
			val = request->getParam("NTP_Server_Address", true)->value();
			strncpy((char*)&config.NTP_Server_Address, val.c_str(), 32);
		}
		if (request->hasParam("NTP_Time_Offset", true)) {
			val = request->getParam("NTP_Time_Offset", true)->value();
			config.NTP_Time_Offset = val.toInt();
		}
		if (request->hasParam("RTC_Time_Offset", true)) {
			val = request->getParam("RTC_Time_Offset", true)->value();
			config.RTC_Time_Offset = val.toInt();
		}
		saveConfig();
		updateRTC();
		request->send(200);
	});
	webServer.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
		response->addHeader("Refresh", "30");
		response->addHeader("Location", "/");
		request->send(response);
		ESP.restart();
	});
	webServer.on("/api/system/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		resetConfig();
		AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
		response->addHeader("Refresh", "30");
		response->addHeader("Location", "/");
		request->send(response);
		ESP.restart();
	});
	webServer.on("/api/raceState", HTTP_GET, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		DynamicJsonDocument resultDoc(4096);
		String resultJsonValue;

		resultDoc["offsetEpochTime"] = offsetEpochTime;
		resultDoc["offsetEpochMillis"] = offsetEpochMillis;
		resultDoc["offsetNTP"] = config.NTP_Time_Offset;
		resultDoc["offsetRTCMillis"] = config.RTC_Time_Offset;
		resultDoc["autoMode"] = config.LapTimer_AutoModeEnabled;
		resultDoc["totalLap"] = config.LapTimer_TotalLap;
		resultDoc["totalInterChange"] = config.LapTimer_TotalInterChange;
		resultDoc["startDelay"] = config.LapTimer_StartDelay;

		resultDoc["autoBTO"] = config.LapTimer_Auto_BTO;
		resultDoc["autoLimit"] = config.LapTimer_Auto_Limit;
		resultDoc["BTO"] = config.LapTimer_BTO;
		resultDoc["limit"] = config.LapTimer_Limit;
		resultDoc["deltaLimit"] = config.LapTimer_Delta_Limit;
		
		resultDoc["totalLane"] = 3;
		
		resultDoc["result"]["currentState"] = raceState.currentState;
		resultDoc["result"]["startTime"] = raceState.startTime;
		resultDoc["result"]["currentTime"] = raceState.currentTime;
		resultDoc["result"]["currentLap"] = raceState.currentLap;
		resultDoc["result"]["currentRacerCount"] = currentRacerCount;
		for (int i = 0; i < 3; i++)
		{
			resultDoc["result"]["racers"][i]["isAvailable"] = raceState.racersState[i].isAvailable;
			resultDoc["result"]["racers"][i]["currentLap"] = raceState.racersState[i].currentLap;
			resultDoc["result"]["racers"][i]["finishTime"] = raceState.racersState[i].finishTime;
			resultDoc["result"]["racers"][i]["isDisqualified"] = raceState.racersState[i].isDisqualified;
			for (int j = 0; j < config.LapTimer_TotalLap; j++)
			{
				resultDoc["result"]["racers"][i]["lapTimes"][j]["currentLane"] = raceState.racersState[i].lapTimes[j].currentLane;
				resultDoc["result"]["racers"][i]["lapTimes"][j]["nextLane"] = raceState.racersState[i].lapTimes[j].nextLane;
				resultDoc["result"]["racers"][i]["lapTimes"][j]["lapTime"] = raceState.racersState[i].lapTimes[j].lapTime;
			}
		}
		serializeJson(resultDoc, resultJsonValue);
#ifdef DEBUG_WEB
		AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resultJsonValue);
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
#else
		request->send(200, "application/json", resultJsonValue);
#endif
	});
	webServer.on("/api/lastResult", HTTP_GET, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
		return request->requestAuthentication();
#endif
	DynamicJsonDocument resultDoc(4096);
	String resultJsonValue;
	resultDoc["currentState"] = lastRaceState.currentState;
	resultDoc["startTime"] = lastRaceState.startTime;
	resultDoc["currentTime"] = lastRaceState.currentTime;
	resultDoc["currentLap"] = lastRaceState.currentLap;
	for (int i = 0; i < 3; i++)
	{
		resultDoc["racers"][i]["isAvailable"] = lastRaceState.racersState[i].isAvailable;
		resultDoc["racers"][i]["currentLap"] = lastRaceState.racersState[i].currentLap;
		resultDoc["racers"][i]["finishTime"] = lastRaceState.racersState[i].finishTime;
		resultDoc["racers"][i]["isDisqualified"] = lastRaceState.racersState[i].isDisqualified;
		for (int j = 0; j < config.LapTimer_TotalLap; j++)
		{
			resultDoc["racers"][i]["lapTimes"][j]["currentLane"] = lastRaceState.racersState[i].lapTimes[j].currentLane;
			resultDoc["racers"][i]["lapTimes"][j]["nextLane"] = lastRaceState.racersState[i].lapTimes[j].nextLane;
			resultDoc["racers"][i]["lapTimes"][j]["lapTime"] = lastRaceState.racersState[i].lapTimes[j].lapTime;
		}
	}
	serializeJson(resultDoc, resultJsonValue);
#ifdef DEBUG_WEB
	AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resultJsonValue);
	response->addHeader("Access-Control-Allow-Origin", "*");
	request->send(response);
#else
	request->send(200, "application/json", resultJsonValue);
#endif
		});

	webServer.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
			return request->requestAuthentication();
#endif
		DynamicJsonDocument resultDoc(1024);
		String resultJsonValue;
		resultDoc["ChipRevision"] = String(ESP.getChipRevision());
		resultDoc["CpuFreqMHz"] = String(ESP.getCpuFreqMHz());
		resultDoc["SketchMD5"] = String(ESP.getSketchMD5());
		resultDoc["SketchSize"] = String(ESP.getSketchSize());
		resultDoc["CycleCount"] = String(ESP.getCycleCount());
		resultDoc["SdkVersion"] = String(ESP.getSdkVersion());
		resultDoc["HeapSize"] = String(ESP.getHeapSize());
		resultDoc["FreeHeap"] = String(ESP.getFreeHeap());
		resultDoc["FreePsram"] = String(ESP.getFreePsram());
		resultDoc["MaxAllocPsram"] = String(ESP.getMaxAllocPsram());
		resultDoc["MaxAllocHeap"] = String(ESP.getMaxAllocHeap());
		resultDoc["FreeSketchSpace"] = String(ESP.getFreeSketchSpace());
		resultDoc["WiFiMacAddress"] = WiFi.macAddress();
		serializeJson(resultDoc, resultJsonValue);
#ifdef DEBUG_WEB
		AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resultJsonValue);
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
#else
		request->send(200, "application/json", resultJsonValue);
#endif
	});

	webServer.on("/api/time", HTTP_GET, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str()))
		return request->requestAuthentication();
#endif
	DynamicJsonDocument resultDoc(1024);
	String resultJsonValue;
	resultDoc["epochTime"] = offsetEpochTime;
	resultDoc["deviceUptime"] = millis();
	resultDoc["epochDeviceMillis"] = offsetEpochMillis;
	resultDoc["RTCTime"] = getCurrentTime();
	serializeJson(resultDoc, resultJsonValue);
#ifdef DEBUG_WEB
	AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resultJsonValue);
	response->addHeader("Access-Control-Allow-Origin", "*");
	request->send(response);
#else
	request->send(200, "application/json", resultJsonValue);
#endif
		});

#ifdef OTA_ENABLED

	webServer.on("/api/update/firmware", HTTP_POST, [](AsyncWebServerRequest* request) {
#ifndef DEBUG_WEB
		if (!request->authenticate(uname.c_str(), pwd.c_str())) {
			request->send(401);
			return;
		}
#endif
		}, [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final)
	{
		handleUploadFirmware(request, filename, index, data, len, final);
	});
#endif

	ws.onEvent(onWsEvent);
	webServer.addHandler(&ws);
	webServer.begin();
	ws.enable(true);
	tmrWSHealth = xTimerCreate(
		"tmrWSHealth", /* name */
		pdMS_TO_TICKS(5000), /* period/time */
		pdTRUE, /* auto reload */
		(void*)1, /* timer ID */
		&tmrWSHealthFun); /* callback */
	if (xTimerStart(tmrWSHealth, 10) != pdPASS) {
		Serial.println("Timer start error");
	}
}

void sendLapTimerStatus() {
	if (!hasWSClient())
		return;
	msgCode = 255;
	setWebSocketHeader(msgCode);
	setWebSocketContent((uint8_t*)&raceState.currentState, 1);
	setWebSocketContent((uint8_t*)&currentRacerCount, 1);
	setWebSocketContent((uint8_t*)&raceState.currentLap, 1);
	setWebSocketContent((uint8_t*)&raceState.startTime, 4);
	setWebSocketContent((uint8_t*)&raceState.currentTime, 4);
	sendAllWebSocketMessage();
}
void sendLapTimerBTO() {
	if (!hasWSClient())
		return;
	msgCode = 254;

	setWebSocketHeader(msgCode);
	setWebSocketContent((uint8_t*)&config.LapTimer_Auto_BTO, 1);
	setWebSocketContent((uint8_t*)&config.LapTimer_Auto_Limit, 1);
	setWebSocketContent((uint8_t*)&config.LapTimer_BTO, 4);
	setWebSocketContent((uint8_t*)&config.LapTimer_Limit, 4);
	setWebSocketContent((uint8_t*)&config.LapTimer_Delta_Limit, 4);
	sendAllWebSocketMessage();
}
void sendRacerStatus(uint8_t racerIdx, int8_t lapIdx) {
	if (!hasWSClient())
		return;
	msgCode = 201;
	long lapTimes = 0;
	if (lapIdx >= 0)
		lapTimes = raceState.racersState[racerIdx].lapTimes[lapIdx].lapTime;
	setWebSocketHeader(msgCode);
	setWebSocketContent((uint8_t*)&racerIdx, 1);
	setWebSocketContent((uint8_t*)&raceState.racersState[racerIdx].isAvailable, 1);
	setWebSocketContent((uint8_t*)&raceState.racersState[racerIdx].isDisqualified, 1);
	setWebSocketContent((uint8_t*)&raceState.racersState[racerIdx].currentLap, 1);
	setWebSocketContent((uint8_t*)&raceState.racersState[racerIdx].finishTime, 4);
	setWebSocketContent((uint8_t*)&lapTimes, 4);
	sendAllWebSocketMessage();
}
bool hasWSClient() {
	return ws.getClients().length() > 0;
}
void resetWebServer() {
	ws.closeAll();
	delay(500);
	webServer.reset();
}
