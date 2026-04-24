#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time_helper.h"

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* FIREBASE_BASE_URL;

const int TOTAL_HISTORY_SLOTS = 288;

bool ensureWiFiConnected();

String makeSlotName(int slot) {
  if (slot < 10) return "00" + String(slot);
  if (slot < 100) return "0" + String(slot);
  return String(slot);
}

bool writeToFirebasePath(const String& path, const String& json) {
  if (!ensureWiFiConnected()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE_URL) + path;

  if (!https.begin(client, url)) {
    Serial.println("Could not connect to Firebase URL.");
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  int httpResponseCode = https.PUT(json);

  Serial.print("HTTP response code for ");
  Serial.print(path);
  Serial.print(": ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = https.getString();
    Serial.println("Firebase response:");
    Serial.println(response);
  } else {
    Serial.print("Upload failed, error: ");
    Serial.println(https.errorToString(httpResponseCode));
  }

  https.end();

  return httpResponseCode > 0;
}

bool writeLatestReading(float temperatureF, float humidity, int slot) {
  String slotName = makeSlotName(slot);
  String timestamp = getTimestampString();

  String json = "{";
  json += "\"slot\": \"" + slotName + "\",";
  json += "\"temperatureF\": " + String(temperatureF, 2) + ",";
  json += "\"humidity\": " + String(humidity, 2) + ",";
  json += "\"timestamp\": \"" + timestamp + "\"";
  json += "}";

  return writeToFirebasePath("/latest.json", json);
}

bool writeHistoryReading(int slot, float temperatureF, float humidity) {
  String slotName = makeSlotName(slot);
  String timestamp = getTimestampString();

  String json = "{";
  json += "\"slot\": \"" + slotName + "\",";
  json += "\"temperatureF\": " + String(temperatureF, 2) + ",";
  json += "\"humidity\": " + String(humidity, 2) + ",";
  json += "\"timestamp\": \"" + timestamp + "\"";
  json += "}";

  return writeToFirebasePath("/history/" + slotName + ".json", json);
}

bool writeCurrentSlotMeta(int slot) {
  String json = String(slot);
  return writeToFirebasePath("/meta/currentSlot.json", json);
}

int loadCurrentSlotFromFirebase() {
  if (!ensureWiFiConnected()) {
    Serial.println("Could not load current slot because Wi-Fi is not connected.");
    return 0;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE_URL) + "/meta/currentSlot.json";

  if (!https.begin(client, url)) {
    Serial.println("Could not connect to Firebase URL for current slot.");
    return 0;
  }

  int httpResponseCode = https.GET();

  Serial.print("HTTP response code for loading current slot: ");
  Serial.println(httpResponseCode);

  int loadedSlot = 0;

  if (httpResponseCode > 0) {
    String response = https.getString();
    Serial.print("Current slot response: ");
    Serial.println(response);

    response.trim();

    if (response != "null") {
      loadedSlot = response.toInt();

      if (loadedSlot < 0 || loadedSlot >= TOTAL_HISTORY_SLOTS) {
        Serial.println("Loaded slot was out of range. Resetting to 0.");
        loadedSlot = 0;
      }
    } else {
      Serial.println("No saved slot found yet. Starting at 0.");
    }
  } else {
    Serial.print("Failed to load current slot, error: ");
    Serial.println(https.errorToString(httpResponseCode));
  }

  https.end();
  return loadedSlot;
}