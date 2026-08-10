/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#include "espMqttClient.h"

#if defined(ARDUINO_ARCH_ESP32)
espMqttClient::espMqttClient(espMqttClientTypes::UseInternalTask useInternalTask)
: MqttClientSetup(useInternalTask)
, _client() {
  _transport = &_client;
}

espMqttClient::espMqttClient(uint8_t priority, uint8_t core)
: MqttClientSetup(espMqttClientTypes::UseInternalTask::YES, priority, core)
, _client() {
  _transport = &_client;
}
#endif
