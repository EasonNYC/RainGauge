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

/**
 * @brief Sends all queued MQTT messages to the broker
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * @param mqttClient Reference to the PubSubClient for MQTT communication
 * @param mqtt_queue Reference to the MqttMessageQueue containing messages to send
 * 
 * This function dequeues all messages from the mqtt_queue and publishes them
 * to the MQTT broker using the provided mqttClient. Each message is sent with
 * a 100ms delay between transmissions. Debug output shows sending progress.
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
 * This function determines and prints the wakeup cause for the ESP32. It handles
 * different wakeup sources:
 * - ESP_SLEEP_WAKEUP_EXT0: Rain gauge trigger (increments rain count, sets activeRain)
 * - ESP_SLEEP_WAKEUP_TIMER: Scheduled timer wakeup (sets timeToUpdate flag)
 * - Other causes: External signals, touchpad, ULP, or normal startup
 * 
 * Side effects:
 * - Increments latest_Raincount if woken by rain gauge
 * - Sets activeRain = true for rain gauge wakeup
 * - Sets timeToUpdate = true for timer wakeup
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

/**
 * @brief Configures ESP32 deep sleep with timer and external GPIO wakeup
 * @param rain_pin The GPIO pin number connected to the rain gauge sensor
 * 
 * This function sets up two wakeup sources for the ESP32:
 * 1. Timer wakeup: Uses the global TIME_TO_SLEEP variable to set sleep duration
 * 2. External GPIO wakeup (EXT0): Configured for the rain gauge pin with LOW trigger
 * 
 * The timer wakeup allows periodic data transmission, while the GPIO wakeup
 * enables immediate response to rain events. Prints sleep configuration details
 * and warnings if timer arguments are invalid.
 */
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