
#include "Arduino.h"
//#include <AsyncTCP.h>
#include <WiFiUdp.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FunctionalInterrupt.h>
#include "inc/MqttMessageQueue.h"


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

//persistent data
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int latest_Raincount = 0;
RTC_DATA_ATTR bool activeRain = false;
RTC_DATA_ATTR bool timeToUpdate = false;

float unit_of_rain = 0.01193;//inches per pulse

template<size_t QUEUE_SIZE>
class Raingauge {
  
public:
  Raingauge(uint8_t reqPin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top) 
  : PIN(reqPin),client(cli),tx_queue(q),topic(top)
  {
    
    pinMode(PIN, INPUT_PULLUP);
    _lastMillis = millis();
  };

  void begin(){
    attachInterrupt(PIN, std::bind(&Raingauge::isr,this), FALLING);
    Serial.printf("Started Raingauge on pin %d\n", PIN);
  }

  ~Raingauge(){
    detachInterrupt(PIN);
  }

  void ARDUINO_ISR_ATTR isr(){
    if(millis()-_lastMillis > 100) {
      _rainBucketsDumped += 1;
      latest_Raincount += _rainBucketsDumped;
      activeRain = true;
      _lastMillis = millis();
    }
  }

  bool isTimeToUpdate() { return timeToUpdate;}

  bool isRaining() { return activeRain;}

  void updateRain(){

    static unsigned long hourCount = 0;

    //if it rained this hour print the stats
    float rainLastHour = 0.0;
    if (isRaining()) {
      rainLastHour = (float)latest_Raincount*unit_of_rain; //inches per bucket
      Serial.printf("Update: Rainfall LastHour=%f inches\n", rainLastHour);
      
      activeRain = false;
      _rainBucketsDumped = 0;
      
    } 

    JsonDocument myObject;
    myObject["rain"] = rainLastHour;
    tx_queue->enqueue(topic.c_str(), myObject);

    latest_Raincount = 0;
    timeToUpdate = false;
  }

  void reportRain(){
      float rainLastHour = (float)latest_Raincount*unit_of_rain;
      Serial.printf("(%dms) Rainfall Report: Detected rain %u times in the last hour\n", millis(), latest_Raincount);
      Serial.printf("(%dms) Rainfall Report: LastHour: %f inches\n", millis(), rainLastHour);
  }

  void handle() {
    if(isTimeToUpdate()){
        reportRain();
        updateRain();
    }
  }

private:
    const uint8_t PIN;
    PubSubClient* client;
    MqttMessageQueue<QUEUE_SIZE>* tx_queue;
    String topic;
    volatile uint32_t _rainBucketsDumped;
    volatile bool _rain = false;
    volatile unsigned long _lastMillis = 0;
};