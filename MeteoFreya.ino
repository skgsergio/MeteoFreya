#include "config.h"

#include <WiFi.h>

/* InfluxDB */
#if INFLUXDB_USE_HTTP
#include <HTTPClient.h>
IPAddress influxdb_ip(INFLUXDB_HOST);
#else
#include <WiFiUDP.h>
#endif

/* DHT sensor */
#if USE_DHT
#include <dht.h>

dht dht_sensor;

#if LOOP_DELAY < 2000
#error DHT sensor needs LOOP_DELAY to be >= 2000
#endif

#endif

/* BMP180 sensor */
#if USE_BMP180
#include <SFE_BMP180.h>

SFE_BMP180 bmp180_sensor;
#endif

/* BH1750 sensor */
#if USE_BH1750
#include <BH1750.h>

BH1750 bh1750_sensor(BH1750_ADDR);
#endif

/* Serial print */
#if SERIAL_DEBUG
#define SERIAL_PRINTLN(MSG) Serial.println(MSG)
#define SERIAL_PRINT(MSG) Serial.print(MSG)
#else
#define SERIAL_PRINTLN(MSG)
#define SERIAL_PRINT(MSG)
#endif

/**
 * Setup
 **/
void setup() {
#if SERIAL_DEBUG
  Serial.begin(115200);
#endif

  SERIAL_PRINTLN("Starting MeteoFreya...");

  /* Connect to WiFi */
  SERIAL_PRINT("Connecting to " + String(WLAN_SSID));
  WiFi.begin(WLAN_SSID, WLAN_KEY);
  while (WiFi.status() != WL_CONNECTED) {
    SERIAL_PRINT(".");
    delay(500);
  }
  SERIAL_PRINTLN(" " + WiFi.localIP());

  /* Initialize sensors */
#if USE_DHT
  SERIAL_PRINTLN("Starting DHT" + String(DHT_MODEL) + "...");
#if DHT_MODEL == 11 || DHT_MODEL == 12
  while (dht_sensor.read11(DHT_PIN) != DHTLIB_OK) {
    delay(2000);
  }
#else
  while (dht_sensor.read(DHT_PIN) != DHTLIB_OK) {
    delay(2000);
  }
#endif
#endif

#if USE_BMP180
  SERIAL_PRINTLN("Starting BMP180...");
  while (!bmp180_sensor.begin()) {
    delay(1000);
  }
#endif

#if USE_BH1750
  SERIAL_PRINTLN("Starting BH1750...");
  bh1750_sensor.begin(BH1750_CONTINUOUS_HIGH_RES_MODE);
#endif

  SERIAL_PRINTLN("MeteoFreya started!");
}

/**
 * Loop
 **/
#if USE_DHT
float dht_temp = 0;
float dht_hum = 0;
float dht_hidx = 0;
int dht_ret = 0;
#endif

#if USE_BMP180
double bmp180_press = 0;
double bmp180_temp = 0;
int bmp180_ret = 0;
#endif

#if USE_BH1750
uint16_t bh1750_lux = 0;
#endif

void loop() {
  delay(LOOP_DELAY);

#if USE_DHT
  dht_ret = readDHT(dht_sensor, DHT_PIN, dht_temp, dht_hum);
  dht_hidx = calcHeatIndex(dht_temp, dht_hum);

  if (dht_ret == DHTLIB_OK) {
    sendLineToInfluxDB(INFLUXDB_DATABASE, "dht,sensor=" + String(SENSOR_NAME) + " temperature=" + String(dht_temp) + ",humidity=" + String(dht_hum) + ",heatindex=" + String(dht_hidx));
  }
#endif

#if USE_BMP180
  bmp180_ret = readBMP180(bmp180_sensor, bmp180_press, bmp180_temp);

  if (bmp180_ret == 0) {
    sendLineToInfluxDB(INFLUXDB_DATABASE, "bmp180,sensor=" + String(SENSOR_NAME) + " pressue=" + String(bmp180_press) + ",temperature=" + String(bmp180_temp));
  }
#endif

#if USE_BH1750
  lux = bh1750_sensor.readLightLevel();

  sendLineToInfluxDB(INFLUXDB_DATABASE, "bh1752,sensor=" + String(SENSOR_NAME) + " lux=" + String(bh1750_lux));
#endif
}

#if USE_DHT
/* Read DHT sensor */
int readDHT(dht sensor, uint8_t pin, float &T, float &H) {
#if DHT_MODEL == 11 || DHT_MODEL == 12
  int ret = sensor.read11(pin);
#else
  int ret = sensor.read(pin);
#endif

  T = sensor.temperature;
  H = sensor.humidity;

  switch (ret) {
  case DHTLIB_OK:
    break;

  case DHTLIB_ERROR_CHECKSUM:
    SERIAL_PRINTLN("[DHT22]: Checksum error");
    break;

  case DHTLIB_ERROR_TIMEOUT:
    SERIAL_PRINTLN("[DHT22]: Timeout error");
    break;

  default:
    SERIAL_PRINTLN("[DHT22]: Unknown error");
    break;
  }

  return ret;
}

/* Calc Heat Index: http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml */
float calcHeatIndex(float T, float H) {
  float hi;

  T = (T * 1.8) + 32;

  hi = 0.5 * (T + 61.0 + ((T - 68.0) * 1.2) + (H * 0.094));

  if (hi > 79) {
    hi = -42.379 +
      2.04901523 * T +
      10.14333127 * H +
      -0.22475541 * T * H +
      -0.00683783 * pow(T, 2) +
      -0.05481717 * pow(H, 2) +
      0.00122874 * pow(T, 2) * H +
      0.00085282 * T * pow(H, 2) +
      -0.00000199 * pow(T, 2) * pow(H, 2);

    if ((H < 13) && (T >= 80.0) && (T <= 112.0)) {
      hi -= ((13.0 - H) * 0.25) * sqrt((17.0 - abs(T - 95.0)) * 0.05882);
    }

    else if ((H > 85.0) && (T >= 80.0) && (T <= 87.0)) {
      hi += ((H - 85.0) * 0.1) * ((87.0 - T) * 0.2);
    }
  }

  return (hi - 32) / 1.8;
}
#endif

#if USE_BMP180
/* Read BMP180 sensor */
int readBMP180(SFE_BMP180 sensor, double &P, double &T) {
  char ret;

  ret = sensor.startTemperature();

  if (ret == 0) {
    SERIAL_PRINTLN("[BPM180]: error starting temperature measurement.");
    return 1;
  }

  delay(ret);

  ret = sensor.getTemperature(T);

  if (ret == 0) {
    SERIAL_PRINTLN("[BPM180]: error retrieving temperature measurement.");
    return 1;
  }

  ret = sensor.startPressure(3);

  if (ret == 0) {
    SERIAL_PRINTLN("[BPM180]: error starting pressure measurement.");
    return 1;
  }

  delay(ret);

  ret = sensor.getPressure(P, T);

  if (ret == 0) {
    SERIAL_PRINTLN("[BPM180]: error retrieving pressure measurement.");
    return 1;
  }

  return 0;
}
#endif

void sendLineToInfluxDB(String database, String line) {
#if INFLUXDB_USE_HTTP
  HTTPClient http;

  http.begin("http://" + influxdb_ip.toString() + ":" + String(INFLUXDB_PORT) + "/write?db=" + INFLUXDB_DATABASE);
  http.addHeader("Content-Type", "text/plain");
  int response = http.POST(line);
  http.end();

  SERIAL_PRINTLN("[InfluxDB:" + String(response) + "]: " + line);
#else
  WiFiUDP udp;
  udp.beginPacket(INFLUXDB_HOST, INFLUXDB_PORT);
  udp.print(line);
  udp.endPacket();

  SERIAL_PRINTLN("[InfluxDB:UDP]: " + line);
#endif
}
