#include <Arduino.h>
#include <secret.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>

// #define HAS_BME680 
#define HAS_BME280


#ifdef HAS_BME680
# define SENSOR_NAME "BME680"
#include "Adafruit_BME680.h"
Adafruit_BME680 bme; // I2C
#endif
#ifdef HAS_BME280
# define SENSOR_NAME "BME280"
#include "Adafruit_BME280.h"
Adafruit_BME280 bme; // I2C
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
Point sensor(SENSOR_NAME);

// BME680 sensor
  
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

    
    bool status;
    #ifdef HAS_BME680
    status = bme.begin();
    #endif
    #ifdef HAS_BME280
    status = bme.begin(0x76, &Wire);
    #endif

    if (!status) {
        Serial.println(F("Could not find a valid BME sensor, check wiring!"));
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);
        while (1);
    }
    // Set up oversampling and filter initialization
    #ifdef HAS_BME680
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
    #endif
    #ifdef HAS_BME280
    bme.setSampling(Adafruit_BME280::MODE_NORMAL, // Operating Mode
                    Adafruit_BME280::SAMPLING_X8, // temperature
                    Adafruit_BME280::SAMPLING_X2, // pressure
                    Adafruit_BME280::SAMPLING_X4, // humidity
                    Adafruit_BME280::FILTER_OFF);
    #endif

}

void loop() {
    // Clear fields for reusing the point. Tags will remain the same as set above.
    sensor.clearFields();

    #ifdef HAS_BME680
    unsigned long endTime = bme.beginReading();
    if (endTime == 0) {
        Serial.println(F("Failed to begin reading :("));
        return;
    }

    if (!bme.endReading()) {
        Serial.println(F("Failed to complete reading :("));
        return;
    }

    sensor.addField("temperature", bme.temperature);
    sensor.addField("pressure", bme.pressure / 100.0);
    sensor.addField("humidity", bme.humidity);
    sensor.addField("gas_resistance", bme.gas_resistance / 1000.0);
    sensor.addField("altitude", bme.readAltitude(SEALEVELPRESSURE_HPA));
    #endif
    #ifdef HAS_BME280
    sensor.addField("temperature", bme.readTemperature());
    sensor.addField("pressure", bme.readPressure() / 100.0);
    sensor.addField("humidity", bme.readHumidity());
    sensor.addField("altitude", bme.readAltitude(SEALEVELPRESSURE_HPA));
    #endif
  
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