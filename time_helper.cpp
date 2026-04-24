#include <Arduino.h>
#include <time.h>

const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO = "PST8PDT,M3.2.0/2,M11.1.0/2";

void syncTime() {
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TZ_INFO, 1);
  tzset();

  Serial.print("Syncing time");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Time synced.");
}

String getTimestampString() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "time_not_set";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}