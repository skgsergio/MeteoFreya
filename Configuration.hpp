#ifndef __CONFIG_H__
#define __CONFIG_H__

#define SENSOR_NAME "home"

/**
 * WiFi Preferences
 **/

#define WLAN_SSID "YourSSID"
#define WLAN_KEY "YourKey"

/**
 * InfluxDB preferences
 **/

#define INFLUXDB_HOST "192.168.1.4:8086"
#define INFLUXDB_AUTH true
#define INFLUXDB_USER "meteofreya"
#define INFLUXDB_PASS "freya1234"
#define INFLUXDB_DATABASE "meteofreya"

/**
 * Hardware preferences
 **/

#define USE_DHT true
#define DHT_MODEL 22
#define DHT_PIN 13

#define USE_BMP180 true
#define BMP180_ALTITUDE 700

#define USE_BH1750 false
#define BH1750_ADDR 0x23

/**
 * Loop wait time
 **/

#define LOOP_DELAY 10000

#endif
