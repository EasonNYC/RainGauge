#ifndef UTILS_H
#define UTILS_H

#include "Arduino.h"
#include "esp_sleep.h"
#include <PubSubClient.h>
#include "MqttMessageQueue.h"

// Forward declarations
extern int latest_Raincount;
extern bool activeRain;
extern bool timeToUpdate;
extern unsigned long long TIME_TO_SLEEP;

//send queued messages to mqtt broker
template<size_t QUEUE_SIZE>
void sendQueuedMessages(PubSubClient& mqttClient, MqttMessageQueue<QUEUE_SIZE>& mqtt_queue) {
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

void configSleepTimer(int rain_pin)
{
  esp_err_t ret = esp_sleep_enable_timer_wakeup(1000000ULL * TIME_TO_SLEEP);
  if(ret == ESP_ERR_INVALID_ARG) {
    Serial.println("WARNING: Sleep timer arg out of bounds");
  }
  Serial.println("ESP32 to sleep for every " + String(TIME_TO_SLEEP) +  " Seconds");

  //config gpio sleep wakeup (for rain gauge bucket)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)rain_pin, 0);
}

#endif