//#include <OneWire.h>
//#include <WebSerial.h>
//#include <Ethernet.h>
//#include <ESPmDNS.h>

#include "Arduino.h"
#include "esp_sleep.h"
#include "esp_bt.h"       // For btStop()

#include "Secrets.h"
#include "time.h"

#include "WifiManager.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"

#include "inc/Rain.h"
#include "inc/Battery.h"
#include "inc/SoilTemp.h"
#include "inc/bmp280.h"

#include <ArduinoOTA.h>

#define DEBUG_MODE_PIN 12
#define GND_TMP_PIN 33
#define BATTERY_PIN A1
#define RAIN_PIN 27

#define MQTT_QUEUE_LENGTH 10

unsigned long long TIME_TO_SLEEP  = 60;        /* Time ESP32 will go to sleep (in seconds). 3600 in an hour */\
bool debug_mode = false;
const char *topic = "backyard/test/";

MqttMessageQueue<MQTT_QUEUE_LENGTH> mqtt_queue;  // max 10 messages
WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espclient;
PubSubClient pub(espclient);

//send queued messages to mqtt broker
void sendQueuedMessages(PubSubClient& mqttClient) {
    MqttMessage msg;
    Serial.printf("(%dms) Sending queued messages...\n",millis());
    while (!mqtt_queue.isEmpty()) {
        if (mqtt_queue.dequeue(msg)) {
            Serial.printf("Sending msg: %s\n",  msg.payload.c_str());
            mqttClient.publish(msg.topic.c_str(), msg.payload.c_str());
            delay(100);
        }
    }
}

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

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("WAKEUP REASON: ");
  switch(wakeup_reason)
  {
    //Wakeup caused by Rain Gauge
    case ESP_SLEEP_WAKEUP_EXT0 : { 
      latest_Raincount++;
      Serial.printf("Rain. Raincount increased to %d\n", latest_Raincount);
      activeRain = true;
      break;
    } 
    
    //Wakeup caused by pre-scheduled timer
    case ESP_SLEEP_WAKEUP_TIMER : { 
      Serial.println("Timer.");
      timeToUpdate = true;

      break;
    }

    //not really used
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Default. Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup() {
  ++bootCount;

  //setup Serial
  Serial.begin(115200);

  //print wakeup reason
  print_wakeup_reason();

  // disable bluetooth
  btStop();

  //setup debug mode pin
  pinMode(DEBUG_MODE_PIN, INPUT_PULLUP);
  
  if (digitalRead(DEBUG_MODE_PIN)) {
    Serial.println("DEBUG MODE: ON");
    debug_mode = true;

    //connect to wifi early
    connectToWifi();
    
    ArduinoOTA.setPort(OTA_PORT); // Port defaults to 3232
    ArduinoOTA.setHostname(OTA_HOSTNAME); // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setPassword(OTA_PASSWORD);     // No authentication by default

    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("OTA Mode Active");

  } else {
    Serial.println("DEBUG MODE: OFF");
    debug_mode = false;
  }
  
  //report persistent data
  Serial.println("Boot count: " + String(bootCount) + "\nRain count: " + String(latest_Raincount));

  //setup sensors
  Serial.printf("(%dms) Setting up... yawn.. i need a coffee.\n", millis());
  my_battery.begin();
  rain_gauge.begin();
  temp_sensor.begin(); 
  bmp_sensor.begin();

  //config sleep timer
  esp_err_t ret = esp_sleep_enable_timer_wakeup(1000000ULL * TIME_TO_SLEEP);
  if(ret == ESP_ERR_INVALID_ARG) {
    Serial.println("WARNING: Sleep timer arg out of bounds");
  }
  Serial.println("ESP32 to sleep for every " + String(TIME_TO_SLEEP) +  " Seconds");

  //config gpio sleep wakeup (for rain gauge bucket)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 0);

}

void loop() {

  //handle sensors
  if (rain_gauge.isTimeToUpdate()) {
    
    connectToWifi();
    //todo: ntp
    if(connectToMqtt()){
      

      rain_gauge.handle();
      my_battery.handle();
      temp_sensor.handle();
      bmp_sensor.handle();

      //send data to mqtt broker
      sendQueuedMessages(pub);
    }
  }

  //handle debug mode or deep sleep  
  if (!debug_mode){
    
    // DEEP SLEEP
    Serial.printf("(%dms) Sleeping now... snores... hehe\n", millis());
    Serial.flush();
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    esp_deep_sleep_start();
  } else {

    //DEBUG MODE 
    ArduinoOTA.handle();
    
    if (digitalRead(DEBUG_MODE_PIN) == 0) {
      debug_mode = false;
      Serial.println("Exiting OTA mode...");
    }
  }

} //end main loop
