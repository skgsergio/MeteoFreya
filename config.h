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

/* InfluxDB through HTTP, if false UDP will be used */
#define INFLUXDB_USE_HTTP true
#define INFLUXDB_DATABASE "meteofreya"

/* InfluxDB host and port, no matter what protocol you are using */
#define INFLUXDB_HOST {192, 168, 1, 4}
#define INFLUXDB_PORT 8086

/**
 * Hardware preferences
 **/

#define USE_DHT true
#define DHT_MODEL 22
#define DHT_PIN 13

#define USE_BMP180 true

#define USE_BH1750 false
#define BH1750_ADDR 0x23

/**
 * Loop wait time
 **/

#define LOOP_DELAY 10000

#endif
