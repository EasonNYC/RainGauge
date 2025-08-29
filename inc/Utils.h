#ifndef UTILS_H
#define UTILS_H

#include "Arduino.h"
#include "esp_sleep.h"
#include <PubSubClient.h>
#include "MqttMessageQueue.h"

// Forward declarations
extern int latest_Raincount;

/**
 * @brief Sends all queued MQTT messages to the broker
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * @param mqttClient Reference to the PubSubClient for MQTT communication
 * @param mqtt_queue Reference to the MqttMessageQueue containing messages to send
 * 
 * Dequeues all messages and publishes to broker with 100ms delays.
 * Debug output shows sending progress.
 */
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

/**
 * @brief Prints the reason why the ESP32 woke up from deep sleep
 * 
 * Determines and prints wakeup cause:
 * - EXT0: Rain gauge (increments count, sets activeRain)
 * - TIMER: Scheduled wakeup (sets timeToUpdate)
 * - Other: External signals, touchpad, ULP, normal startup
 * 
 * Side effects modify global rain/timer flags.
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
      break;
    } 
    
    //Wakeup caused by pre-scheduled timer
    case ESP_SLEEP_WAKEUP_TIMER : { 
      Serial.println("Timer.");
      // SensorScheduler now handles timing logic
      break;
    }

    //not really used
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Default. Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


#endif