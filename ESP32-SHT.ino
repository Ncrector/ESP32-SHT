#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

const char* WIFI_SSID = "Staats Deco";
const char* WIFI_PASSWORD = "Laplace1";

const char* FIREBASE_URL = "https://sht-31-561cc-default-rtdb.firebaseio.com/readings.json";

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const unsigned long READ_DELAY_MS = 300000;

Adafruit_SHT31 sht31 = Adafruit_SHT31();

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

bool uploadToFirebase(float temperatureF, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
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
  }

  WiFiClientSecure client;
  client.setInsecure();  // quick test version

  HTTPClient https;

  if (!https.begin(client, FIREBASE_URL)) {
    Serial.println("Could not connect to Firebase URL.");
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"temperatureF\": " + String(temperatureF, 2) + ",";
  json += "\"humidity\": " + String(humidity, 2) + ",";
  json += "\"timestamp\": " + String(millis());
  json += "}";

  int httpResponseCode = https.POST(json);

  Serial.print("HTTP response code: ");
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
}

void loop() {
  float temperatureC = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  if (!isnan(temperatureC) && !isnan(humidity)) {
    float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;

    Serial.print("Temperature: ");
    Serial.print(temperatureF);
    Serial.println(" F");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    bool success = uploadToFirebase(temperatureF, humidity);

    if (success) {
      Serial.println("Upload succeeded.");
    } else {
      Serial.println("Upload failed.");
    }

    Serial.println();
  } else {
    Serial.println("Failed to read sensor.");
  }

  delay(READ_DELAY_MS);
}