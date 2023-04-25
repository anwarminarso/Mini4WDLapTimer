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
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "TypeDefStruct.h"
#include "GlobalVariables.h"
#include "ClientAPI.h"
#include "Configuration.h"
#include "Utils.h"
#include "RaceState.h"

WiFiClient espClient;
PubSubClient client(espClient);

String mqtt_id;
String publishStateTopic;
String publishRaceTopic;
String publishRacerTopic;
String subscribeTopic;
String resultTopic;
String mqttUsr;
String mqttPwd;
String mqttServerAddr;
bool mqttEnabled;
ulong teleInterval;
ulong latestTele;
ulong currentMillis;

StaticJsonDocument<1024> pubDoc;
StaticJsonDocument<512> subDoc;

#define MQTT_RESULT_MSG_DATA_ERROR			"Data format error.."
#define MQTT_RESULT_MSG_PARSING_ERROR		"Error parsing data.."
#define MQTT_RESULT_MSG_CMD_NOT_SUPPORTED	"Command not supported"
#define MQTT_RESULT_MSG_OK					"OK"

void publishResult(const char* msg) {
	client.publish(resultTopic.c_str(), msg);
}
void clientAPI_CB(char* topic, byte* message, unsigned int length) {
	String msgValue;

	for (int i = 0; i < length; i++) {
		msgValue += (char)message[i];
	}
	if (String(topic).equals(subscribeTopic)) {
		DeserializationError error = deserializeJson(subDoc, msgValue);
		if (error) {
			publishResult(MQTT_RESULT_MSG_DATA_ERROR);
			return;
		}
		try
		{
			uint8_t cmd = 0;
			if (subDoc["cmd"].is<uint8_t>())
				cmd = subDoc["cmd"].as<uint8_t>();
			switch (cmd)
			{
			//case 5: //write single coil
			//{
			//	uint8_t reg = 0;
			//	uint8_t val = 0;
			//	if (!subDoc["reg"].is<uint8_t>() || !subDoc["val"].is<uint8_t>()) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	reg = subDoc["reg"].as<uint8_t>();
			//	val = subDoc["val"].as<uint8_t>();
			//	if (val > 1)
			//		val = 1;
			//	if (reg >= NUM_DISCRETE_OUTPUT) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	*bool_output[reg / 8][reg % 8] = val;
			//	DOUT_Values[reg] = val;
			//	publishResult(MQTT_RESULT_MSG_OK);
			//}
			//break;
			//case 6: //Write Single Register
			//{
			//	uint8_t reg = 0;
			//	uint16_t val = 0;
			//	if (!subDoc["reg"].is<uint8_t>() || !subDoc["reg"].is<uint16_t>()) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	reg = subDoc["reg"].as<uint8_t>();
			//	val = subDoc["val"].as<uint16_t>();
			//	if (val > 1)
			//		val = 1;
			//	if (reg >= NUM_ANALOG_OUTPUT) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	*int_output[reg] = val;
			//	AOUT_Values[reg] = val;
			//	publishResult(MQTT_RESULT_MSG_OK);
			//}
			//break;
			//case 16: //Write Multiple Register
			//{
			//	JsonArray regs;
			//	JsonArray vals;
			//	if (!subDoc["regs"].is<JsonArray>() || !subDoc["vals"].is<JsonArray>()) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	regs = subDoc["regs"].as<JsonArray>();
			//	vals = subDoc["vals"].as<JsonArray>();
			//	if (regs.size() != vals.size()) {
			//		publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//		return;
			//	}
			//	size_t len = regs.size();
			//	for (uint8_t i = 0; i < len; i++)
			//	{
			//		if (!regs[i].is<uint8_t>() || !vals[i].is<uint16_t>() || regs[i] >= NUM_ANALOG_OUTPUT) {
			//			publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
			//			return;
			//		}
			//	}
			//	for (uint8_t i = 0; i < len; i++)
			//	{
			//		uint8_t reg = regs[i].as<uint8_t>();
			//		uint16_t val = vals[i].as<uint16_t>();
			//		*int_output[reg] = val;
			//		AOUT_Values[reg] = val;
			//	}
			//	publishResult(MQTT_RESULT_MSG_OK);
			//}
			//break;
			default:
				publishResult(MQTT_RESULT_MSG_CMD_NOT_SUPPORTED);
				break;
			}
		}
		catch (const std::exception&)
		{
			publishResult(MQTT_RESULT_MSG_PARSING_ERROR);
		}
	}
}

void reconnectClientAPI() {
	// Loop until we're reconnected
	if (WiFi.isConnected() && !client.connected()) {
	 //if (!client.connected()) {
		Serial.print("Attempting MQTT connection: ");
		// Attempt to connect
		if (client.connect(mqtt_id.c_str(), mqttUsr.c_str(), mqttPwd.c_str())) {
			Serial.println("connected");
			// Subscribe
			client.subscribe(subscribeTopic.c_str());
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println();
		}
	}
}


void initClientAPI() {
	teleInterval = config.MQTT_Tele_Interval * 1000;
	mqttEnabled = config.MQTT_Enabled > 0;
	if (!mqttEnabled)
		return;
	String macAddress = getWifiMacAddress();
	mqtt_id = "ESP-LAPTIMER-";
	mqtt_id += macAddress;
	publishStateTopic = config.MQTT_Root_Topic;
	publishStateTopic += "/state";
	publishRaceTopic = config.MQTT_Root_Topic;
	publishRaceTopic += "/race";
	subscribeTopic = config.MQTT_Root_Topic;
	subscribeTopic += "/command";
	resultTopic = config.MQTT_Root_Topic;
	resultTopic += "/result";
	mqttServerAddr = String(config.MQTT_Server_Address);
	mqttServerAddr.trim();

	mqttUsr = String(config.MQTT_Server_UserName);
	mqttUsr.trim();
	mqttPwd = String(config.MQTT_Server_Password);
	mqttPwd.trim();


	client.setServer(mqttServerAddr.c_str(), config.MQTT_Server_Port);
	client.setCallback(clientAPI_CB);
}

void clientAPILoop() {
	if (!mqttEnabled)
		return;
	if (!WiFi.isConnected())
		return;
	if (client.connected()) {
		client.loop();
		currentMillis = millis();
		if (currentMillis - latestTele > teleInterval)
		{
			publishStateClientAPI();
			latestTele = currentMillis;
		}
	}
	else {
		reconnectClientAPI();
	}
}

void publishStateClientAPI() {
	String resultJsonValue;
	/*for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
	{
		pubDoc["DiscreteInputs"][i] = *bool_input[i / 8][i % 8];
	}

	for (int i = 0; i < NUM_ANALOG_INPUT; i++)
	{
		pubDoc["InputRegisters"][i] = *int_input[i];
	}

	for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
	{
		pubDoc["Coils"][i] = *bool_output[i / 8][i % 8];
	}

	for (int i = 0; i < NUM_ANALOG_OUTPUT; i++)
	{
		pubDoc["HoldingRegisters"][i] = *int_output[i];
	}*/

	serializeJson(pubDoc, resultJsonValue);

	if (!client.publish(publishStateTopic.c_str(), resultJsonValue.c_str()))
		Serial.println("FAILED PUBLISH");
}
void publishRaceClientAPI() {
	String resultJsonValue;
	/*for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
	{
		pubDoc["DiscreteInputs"][i] = *bool_input[i / 8][i % 8];
	}

	for (int i = 0; i < NUM_ANALOG_INPUT; i++)
	{
		pubDoc["InputRegisters"][i] = *int_input[i];
	}

	for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
	{
		pubDoc["Coils"][i] = *bool_output[i / 8][i % 8];
	}

	for (int i = 0; i < NUM_ANALOG_OUTPUT; i++)
	{
		pubDoc["HoldingRegisters"][i] = *int_output[i];
	}*/

	serializeJson(pubDoc, resultJsonValue);

	if (!client.publish(publishRaceTopic.c_str(), resultJsonValue.c_str()))
		Serial.println("FAILED PUBLISH");
}

void sendMqttDeviceState() {
	DynamicJsonDocument resultDoc(1024);
	String resultJsonValue;

	resultDoc["offsetEpochTime"] = offsetEpochTime;
	resultDoc["offsetEpochMillis"] = offsetEpochMillis;
	resultDoc["autoMode"] = config.LapTimer_AutoModeEnabled;
	resultDoc["totalLap"] = config.LapTimer_TotalLap;
	resultDoc["totalInterChange"] = config.LapTimer_TotalInterChange;
	resultDoc["startDelay"] = config.LapTimer_StartDelay;
	resultDoc["totalLane"] = 3;

	resultDoc["result"]["currentState"] = raceState.currentState;
	resultDoc["result"]["startTime"] = raceState.startTime;
	resultDoc["result"]["currentTime"] = raceState.currentTime;
	resultDoc["result"]["currentLap"] = raceState.currentLap;
	for (int i = 0; i < 3; i++)
	{
		resultDoc["result"]["racers"][i]["isAvailable"] = raceState.racersState[i].isAvailable;
		resultDoc["result"]["racers"][i]["currentLap"] = raceState.racersState[i].currentLap;
		resultDoc["result"]["racers"][i]["finishTime"] = raceState.racersState[i].finishTime;
		for (int j = 0; j < config.LapTimer_TotalLap; j++)
		{
			resultDoc["result"]["racers"][i]["lapTimes"][j]["currentLane"] = raceState.racersState[i].lapTimes[j].currentLane;
			resultDoc["result"]["racers"][i]["lapTimes"][j]["nextLane"] = raceState.racersState[i].lapTimes[j].nextLane;
			resultDoc["result"]["racers"][i]["lapTimes"][j]["lapTime"] = raceState.racersState[i].lapTimes[j].lapTime;
		}
	}

	resultDoc["lastResult"]["currentState"] = lastRaceState.currentState;
	resultDoc["lastResult"]["startTime"] = lastRaceState.startTime;
	resultDoc["lastResult"]["currentTime"] = lastRaceState.currentTime;
	resultDoc["lastResult"]["currentLap"] = lastRaceState.currentLap;
	for (int i = 0; i < 3; i++)
	{
		resultDoc["lastResult"]["racers"][i]["isAvailable"] = lastRaceState.racersState[i].isAvailable;
		resultDoc["lastResult"]["racers"][i]["currentLap"] = lastRaceState.racersState[i].currentLap;
		resultDoc["lastResult"]["racers"][i]["finishTime"] = lastRaceState.racersState[i].finishTime;
		for (int j = 0; j < config.LapTimer_TotalLap; j++)
		{
			resultDoc["lastResult"]["racers"][i]["lapTimes"][j]["currentLane"] = lastRaceState.racersState[i].lapTimes[j].currentLane;
			resultDoc["lastResult"]["racers"][i]["lapTimes"][j]["nextLane"] = lastRaceState.racersState[i].lapTimes[j].nextLane;
			resultDoc["lastResult"]["racers"][i]["lapTimes"][j]["lapTime"] = lastRaceState.racersState[i].lapTimes[j].lapTime;
		}
	}
	serializeJson(resultDoc, resultJsonValue);

	if (!client.publish(publishRaceTopic.c_str(), resultJsonValue.c_str()))
		Serial.println("FAILED PUBLISH");
}
void sendMqttLapTimerStatus() {
	if (!mqttEnabled)
		return;

	DynamicJsonDocument resultDoc(1024);
	String resultJsonValue;

	resultDoc["currentState"] = raceState.currentState;
	resultDoc["currentLap"] = raceState.currentLap;
	resultDoc["startTime"] = raceState.startTime;
	resultDoc["currentTime"] = raceState.currentTime;
	
	serializeJson(resultDoc, resultJsonValue);

	if (!client.publish(publishRaceTopic.c_str(), resultJsonValue.c_str()))
		Serial.println("FAILED PUBLISH");
	
}
void sendMqttRacerStatus(uint8_t racerIdx, int8_t lapIdx) {
	if (!mqttEnabled)
		return;

}