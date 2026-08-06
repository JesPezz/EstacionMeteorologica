# Changelog

All notable changes to this project are documented in this file.

## v4.3.4.1-MQTT - 2026-08-06

Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.4.1-MQTT

Summary
- Bumped firmware version to `v4.3.4.1-MQTT` and published updated firmware binary.
- Refactored LED control module to use FreeRTOS task-based signaling via `led_task.cpp` and removed obsolete `led.cpp`.
- Replaced legacy LED helper calls with `signalLed(...)` and migrated include references to `led_task.h`.
- Preserved original LED timing semantics for success, progress, and error states.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware to prevent OTA loops and ensure the version tag matches the code.

## v4.3.4-MQTT - 2026-08-06

Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.2-MQTT

Summary
- Re-enabled live SSE updates from the ESP32 frontend with `EventSource('/events')` and reliable event listeners.
- Ensured `/sensor_data` always returns `battery_voltage` and `battery_status` for correct battery UI rendering.
- Added automatic dashboard startup on page load and resilient JSON parse handling in the frontend.
- Updated Wi-Fi scan endpoint to use asynchronous scanning and avoid blocking HTTP request handlers.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware for live sensor updates and battery state support.
- Related PR: https://github.com/JesPezz/EstacionMeteorologica/pull/5 (draft)

## v4.3.1-MQTT - 2026-08-05

Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.1-MQTT

Summary
- Embeded critical CSS into the frontend (data/index.html) so the UI renders correctly offline and no longer depends on external CDN for layout.
- Hardened frontend JavaScript JSON parsing and SSE handling to avoid the UI freezing when device responses are malformed.
- Fixed /events registration in the web server so AsyncEventSource keeps connections open (prevents SSE disconnects).
- Added battery monitoring fields (battery_voltage, battery_status) to the sensor API and displayed them in the UI.
- Improved logging for resets and voltage diagnostics.
- Reduced redundant NVS/spiffs writes to extend flash life.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware for server-side (web_server.cpp) fixes to take effect.
- Related PR: https://github.com/JesPezz/EstacionMeteorologica/pull/5 (draft)

