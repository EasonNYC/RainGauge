#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

//WIFI
const char* WIFI_SSID = "YourWiFiNetworkName";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* LOCAL_IP = "192.168.1.XXX";
const char* GATEWAY_IP = "192.168.1.1";
const char* SUBNET_MASK = "255.255.255.0";
const char* DNS_SERVER = "192.168.1.1";
const int   WIFI_CHANNEL = 6;
uint8_t     WIFI_BSSID[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

//OTA
const int   OTA_PORT = 3232;
const char* OTA_HOSTNAME = "youresp32hostname";
const char* OTA_PASSWORD = "yourotapassword";


// MQTT Broker
const char* mqtt_broker = "192.168.1.XXX";
const int mqtt_port = 1883;
//const char *mqtt_username = "yourmqttusername";
//const char *mqtt_password = "yourmqttpassword";

#endif