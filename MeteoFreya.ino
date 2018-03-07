#include "Configuration.hpp"

#include <esp32-hal-log.h>
#include <WiFi.h>
#include <Wire.h>

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


/**
 * Setup
 **/
void setup() {
  log_i("Starting MeteoFreya...");

  /* Initialize sensors */
#if USE_DHT
  log_i("Starting DHT%d...", DHT_MODEL);
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
  log_i("Starting BMP180...");
  while (!bmp180_sensor.begin()) {
    delay(1000);
  }
#endif

#if USE_BH1750
  log_i("Starting BH1750...");
  bh1750_sensor.begin(BH1750_CONTINUOUS_HIGH_RES_MODE);
#endif

  connectToWiFi();
}

/**
 * Connect to WiFi
 **/
void connectToWiFi() {
  log_i("Connecting to '%s'...", WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_KEY);
  WiFi.setHostname("MeteoFreya");

  while ((WiFi.status() != WL_CONNECTED) &&
         (WiFi.status() != WL_NO_SSID_AVAIL) &&
         (WiFi.status() != WL_CONNECT_FAILED)) {
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED)  {
    log_i("Got IP: %s", WiFi.localIP().toString().c_str());
  }
  else {
    log_e("Connection failed. Restarting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }
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

  /* Check WiFi status */
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

#if USE_DHT
  /* Read DHT sensor */
  dht_ret = readDHT(dht_sensor, DHT_PIN, dht_temp, dht_hum);
  dht_hidx = calcHeatIndex(dht_temp, dht_hum);

  if (dht_ret == DHTLIB_OK) {
    sendLineToInfluxDB(INFLUXDB_DATABASE, "dht,sensor=" + String(SENSOR_NAME) + " temperature=" + String(dht_temp) + ",humidity=" + String(dht_hum) + ",heatindex=" + String(dht_hidx));
  }
#endif

#if USE_BMP180
  /* Read BMP180 sensor */
  bmp180_ret = readBMP180(bmp180_sensor, bmp180_press, bmp180_temp, BMP180_ALTITUDE);

  if (bmp180_ret == 0) {
    sendLineToInfluxDB(INFLUXDB_DATABASE, "bmp180,sensor=" + String(SENSOR_NAME) + " pressue=" + String(bmp180_press) + ",temperature=" + String(bmp180_temp));
  }
#endif

#if USE_BH1750
  /* Read BH1750 sensor */
  lux = bh1750_sensor.readLightLevel();

  sendLineToInfluxDB(INFLUXDB_DATABASE, "bh1752,sensor=" + String(SENSOR_NAME) + " lux=" + String(bh1750_lux));
#endif
}

#if USE_DHT
/**
 * DHT Sensor
 **/
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
    log_e("Checksum error.");
    break;

  case DHTLIB_ERROR_TIMEOUT:
    log_e("Timeout error.");
    break;

  default:
    log_e("Unknown error.");
    break;
  }

  return ret;
}

/**
 * Calc Heat Index
 * http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
 **/
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
/**
 * BMP180 Sensor
 **/
int readBMP180(SFE_BMP180 sensor, double &P, double &T, double A) {
  char ret;

  ret = sensor.startTemperature();

  if (ret == 0) {
    log_e("Error starting temperature measurement.");
    Wire.reset();
    return 1;
  }

  delay(ret);

  ret = sensor.getTemperature(T);

  if (ret == 0) {
    log_e("Error retrieving temperature measurement.");
    Wire.reset();
    return 1;
  }

  ret = sensor.startPressure(3);

  if (ret == 0) {
    log_e("Error starting pressure measurement.");
    Wire.reset();
    return 1;
  }

  delay(ret);

  ret = sensor.getPressure(P, T);

  if (ret == 0) {
    log_e("Error retrieving pressure measurement.");
    Wire.reset();
    return 1;
  }

  P = sensor.sealevel(P, A);

  return 0;
}
#endif

/**
 * Send line to InfluxDB
 **/
void sendLineToInfluxDB(String database, String line) {
#if INFLUXDB_USE_HTTP
  HTTPClient http;

  http.begin("http://" + influxdb_ip.toString() + ":" + String(INFLUXDB_PORT) + "/write?db=" + INFLUXDB_DATABASE);
  http.addHeader("Content-Type", "text/plain");
  int response = http.POST(line);
  http.end();

  log_i("HTTP(%d) -> %s", response, line.c_str());
#else
  WiFiUDP udp;
  udp.beginPacket(INFLUXDB_HOST, INFLUXDB_PORT);
  udp.print(line);
  udp.endPacket();

  log_i("UDP -> %s", line.c_str());
#endif
}
