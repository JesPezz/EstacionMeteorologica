/*
Copyright (c) 2022 Bert Melis. All rights reserved.

API is based on the original work of Marvin Roger:
https://github.com/marvinroger/async-mqtt-client

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#pragma once

#if defined(ARDUINO_ARCH_ESP32)
#include "Transport/ClientSync.h"
#endif

#include "MqttClientSetup.h"

#if defined(ARDUINO_ARCH_ESP32)
class espMqttClient : public MqttClientSetup<espMqttClient> {
 public:
  explicit espMqttClient(espMqttClientTypes::UseInternalTask useInternalTask);
  explicit espMqttClient(uint8_t priority = 1, uint8_t core = 1);

 protected:
  espMqttClientInternals::ClientSync _client;
};
#endif
