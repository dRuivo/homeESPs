#include <Arduino.h>
#include <secret.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>

#define BME680_NAME "BME680"

#ifdef BME680_NAME
#include "Adafruit_BME680.h"
#endif

#define SEALEVELPRESSURE_HPA (1013.25)

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif
  
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Time zone info
#define TZ_INFO "UTC1"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor(BME680_NAME);

// BME680 sensor
Adafruit_BME680 bme; // I2C
  
void setup() {
    Serial.begin(115200);

    // Setup wifi
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");


    // Check server connection
    if (client.validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    } else {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }

    // Add tags to the data point
    sensor.addTag("device", DEVICE);


    if (!bme.begin()) {
        Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
        while (1);
    }
    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop() {
    // Clear fields for reusing the point. Tags will remain the same as set above.
    sensor.clearFields();

    unsigned long endTime = bme.beginReading();
    if (endTime == 0) {
        Serial.println(F("Failed to begin reading :("));
        return;
    }
    Serial.print(F("Reading started at "));
    Serial.print(millis());
    Serial.print(F(" and will finish at "));
    Serial.println(endTime);

    if (!bme.endReading()) {
        Serial.println(F("Failed to complete reading :("));
        return;
    }

    sensor.addField("temperature", bme.temperature);
    sensor.addField("pressure", bme.pressure / 100.0);
    sensor.addField("humidity", bme.humidity);
    sensor.addField("gas_resistance", bme.gas_resistance / 1000.0);
    sensor.addField("altitude", bme.readAltitude(SEALEVELPRESSURE_HPA));
  
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());
  
    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("Wifi connection lost");
    }
  
    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  
    Serial.println("Waiting 1 second");
    delay(1000);
}