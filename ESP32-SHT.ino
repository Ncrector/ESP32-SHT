#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <time.h>

const char* WIFI_SSID = "Staats Deco";
const char* WIFI_PASSWORD = "Laplace1";

const char* FIREBASE_BASE_URL = "https://sht-31-561cc-default-rtdb.firebaseio.com";
const int TOTAL_HISTORY_SLOTS = 288;

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const unsigned long READ_DELAY_MS = 300000;

const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO = "PST8PDT,M3.2.0/2,M11.1.0/2";

Adafruit_SHT31 sht31 = Adafruit_SHT31();

int currentSlot = 0;

String makeSlotName(int slot) {
  if (slot < 10) return "00" + String(slot);
  if (slot < 100) return "0" + String(slot);
  return String(slot);
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("Wi-Fi disconnected. Reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi reconnection failed.");
    return false;
  }

  Serial.println("Wi-Fi reconnected.");
  return true;
}

bool writeToFirebasePath(const String& path, const String& json) {
  if (!ensureWiFiConnected()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // quick test version

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