# Code Citations

## License: desconocido
https://github.com/lhm0/rotating_display/tree/6402071a5e72a914f805e080af4f23e569447247/RD40_ESP8266/src/my_ESP.cpp

```
);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");
      attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
```

