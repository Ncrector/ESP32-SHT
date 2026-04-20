#include <Wire.h>
#include <Adafruit_SHT31.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  Serial.println("Starting sensor...");

  if (!sht31.begin(0x44)) {
    Serial.println("Sensor not found at 0x44");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Sensor found.");
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
    Serial.println();
  } else {
    Serial.println("Failed to read sensor.");
  }

  delay(3000);
}