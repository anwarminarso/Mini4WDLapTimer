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
#include "Utils.h"

String getChipID() {
	uint64_t id = ESP.getEfuseMac();
	uint16_t chip_h = (uint16_t)(id >> 32);
	uint32_t chip_d = (uint32_t)id;
	char result[13];
	sprintf(result, "%04X%08X", chip_h, chip_d);
	return String(result);
}
String getWifiMacAddress() {
	String macAddress = WiFi.macAddress();
	macAddress.remove(14, 1);
	macAddress.remove(11, 1);
	macAddress.remove(8, 1);
	macAddress.remove(5, 1);
	macAddress.remove(2, 1);
	macAddress.toUpperCase();
	return macAddress;
}
String trimChar(char* value) {
	String result = String(value);
	result.trim();
	return result;
}