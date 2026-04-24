#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "time_helper.h"
#include "firebase_helper.h"
#include "wifi_helper.h"

const char* WIFI_SSID = "Staats Deco";
const char* WIFI_PASSWORD = "Laplace1";

const char* FIREBASE_BASE_URL = "https://sht-31-561cc-default-rtdb.firebaseio.com";
const int TOTAL_HISTORY_SLOTS = 288;

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const unsigned long READ_DELAY_MS = 300000;

Adafruit_SHT31 sht31 = Adafruit_SHT31();

int currentSlot = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Starting sensor...");

  if (!sht31.begin(0x44)) {
    Serial.println("Sensor not found at 0x44");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Sensor found.");

  connectToWiFi();
  syncTime();

  currentSlot = loadCurrentSlotFromFirebase();
  Serial.print("Starting from slot: ");
  Serial.println(makeSlotName(currentSlot));
}

void loop() {
  float temperatureC = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  if (!isnan(temperatureC) && !isnan(humidity)) {
    float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
    String slotName = makeSlotName(currentSlot);

    Serial.print("Temperature: ");
    Serial.print(temperatureF);
    Serial.println(" F");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Writing to slot: ");
    Serial.println(slotName);

    bool latestSuccess = writeLatestReading(temperatureF, humidity, currentSlot);
    bool historySuccess = writeHistoryReading(currentSlot, temperatureF, humidity);

    if (latestSuccess && historySuccess) {
      Serial.println("Upload succeeded.");

      currentSlot++;
      if (currentSlot >= TOTAL_HISTORY_SLOTS) {
        currentSlot = 0;
      }

      bool metaSuccess = writeCurrentSlotMeta(currentSlot);

      if (metaSuccess) {
        Serial.print("Next slot saved as: ");
        Serial.println(makeSlotName(currentSlot));
      } else {
        Serial.println("Failed to save next slot metadata.");
      }
    } else {
      Serial.println("Upload failed.");
    }

    Serial.println();
  } else {
    Serial.println("Failed to read sensor.");
  }

  delay(READ_DELAY_MS);
}