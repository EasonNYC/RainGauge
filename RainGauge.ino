//#include <OneWire.h>
//#include <WebSerial.h>
//#include <Ethernet.h>
//#include <ESPmDNS.h>

#include "Arduino.h"
#include "esp_bt.h"       // For btStop()

#include <WiFi.h>
#include <PubSubClient.h>

#include "Secrets.h"

#include "inc/WifiManager.h"
#include "inc/MqttMessageQueue.h"
#include "inc/Rain.h"
#include "inc/Battery.h"
#include "inc/SoilTemp.h"
#include "inc/BMP280.h"
#include "inc/OTA.h"
#include "inc/DebugManager.h"
#include "inc/Utils.h"
#include "inc/SensorScheduler.h"

#define DEBUG_MODE_PIN 12
#define GND_TMP_PIN 33
#define BATTERY_PIN A1
#define RAIN_PIN 27

#define MQTT_QUEUE_LENGTH 10

const char *topic = "backyard/test/";

MqttMessageQueue<MQTT_QUEUE_LENGTH> mqtt_queue;  // max 10 messages
WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espclient;
PubSubClient pub(espclient);


//connect to wifi
void connectToWifi(){

  Serial.printf("(%dms) WIFI...",millis());
  wifi.setStaticIP(LOCAL_IP,GATEWAY_IP, SUBNET_MASK, DNS_SERVER);
  wifi.setFastConnect(WIFI_BSSID, WIFI_CHANNEL);
  wifi.connect(bootCount);

}

//connect to mqtt
bool connectToMqtt() {

  bool ret = false;

  pub.setServer(mqtt_broker, mqtt_port);
  Serial.printf("(%dms) MQTT...",millis());
  
  int attempts = 0;
  const int maxAttempts = 5;
  
  while (!pub.connected() && attempts < maxAttempts) {
    attempts++;
    
    if (pub.connect("esp32Client")) {
      Serial.print("CONNECTED.\n");
      ret = true; 
    } else {
      Serial.printf("mqtt failed, rc=%d (attempt %d/%d)\n", pub.state(), attempts, maxAttempts);
      
      if (attempts < maxAttempts) {
        Serial.println("Reconnecting to WiFi...");
        wifi.connect(bootCount);
        Serial.println("Trying MQTT again in 5 seconds");
        delay(5000);
      } else {
        Serial.println("Failed to connect to MQTT after 5 attempts");
      }
    }
  }

  return ret;
}

//sensors
battery<MQTT_QUEUE_LENGTH> my_battery(BATTERY_PIN, &pub, &mqtt_queue, "backyard/test/");
Raingauge<MQTT_QUEUE_LENGTH> rain_gauge(RAIN_PIN, &pub,&mqtt_queue,"backyard/test/");
Tempsensor<MQTT_QUEUE_LENGTH> temp_sensor(GND_TMP_PIN, &pub,&mqtt_queue,"backyard/test/");
bmp280sensor<MQTT_QUEUE_LENGTH> bmp_sensor(&pub,&mqtt_queue,"backyard/test/");

//OTA manager
OTAManager ota;

//Debug manager
DebugManager dm(DEBUG_MODE_PIN, &ota);

//Sensor scheduler
SensorScheduler sensorScheduler;


void setup() {
  ++bootCount;

  //setup Serial
  Serial.begin(115200);

  //print wakeup reason
  print_wakeup_reason();

  // disable bluetooth
  btStop();

  //setup debug mode
  if(dm.checkDebugModePin()) {
    dm.startDebugMode(connectToWifi, OTA_PORT, OTA_HOSTNAME, OTA_PASSWORD);
  }
  
  //report persistent data
  Serial.println("Boot count: " + String(bootCount) + "\nRain count: " + String(latest_Raincount));

  //setup sensors with scheduler
  Serial.printf("(%dms) Setting up... yawn.. i need a coffee.\n", millis());
  sensorScheduler.addSensor(&my_battery);
  sensorScheduler.addSensor(&rain_gauge);
  sensorScheduler.addSensor(&temp_sensor);
  sensorScheduler.addSensor(&bmp_sensor);

  // Sleep timer will be configured dynamically in loop() based on sensor needs
  // Still need to configure rain pin for ext0 wakeup
  esp_sleep_enable_ext0_wakeup((gpio_num_t)RAIN_PIN, 0);

}


void loop() {

  //handle sensors
  if (sensorScheduler.hasDataToSend()) {
    
    connectToWifi();
    //todo: ntp here
    if(connectToMqtt()){
      
      //collect sensor data from all ready sensors
      sensorScheduler.printStatus();
      sensorScheduler.checkAndUpdateAll();

      //send data to mqtt broker
      sendQueuedMessages(pub, mqtt_queue);
    }
  }

  //handle debug mode or dynamic deep sleep
  unsigned long sleepTime = sensorScheduler.getNextWakeTime();
  sensorScheduler.prepareSleep(sleepTime);
  
  // Configure dynamic sleep timer based on sensor needs
  esp_err_t ret = esp_sleep_enable_timer_wakeup(1000000ULL * (sleepTime / 1000));
  if(ret == ESP_ERR_INVALID_ARG) {
    Serial.println("WARNING: Sleep timer arg out of bounds");
  }
  Serial.printf("ESP32 to sleep for %lu ms\n", sleepTime);
  
  dm.handle(sleepTime);

} //end main loop
