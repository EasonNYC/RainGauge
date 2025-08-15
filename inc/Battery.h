#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"

/* NOTE: The ADC doesnt work while WiFi is on. So the sampling happens in begin() and the reporting happens in the handle() function.
*/

template<size_t QUEUE_SIZE>
class battery {
private:
    int total;              // the running total
    float average;          // the average
    float vbat;
    int battery_inputPin;
    int battery_numReadings;
    PubSubClient* client;
    MqttMessageQueue<QUEUE_SIZE>* tx_queue;
    String topic;

public:
    battery(uint8_t pin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
        : battery_inputPin(pin), tx_queue(q), total(0), average(0.0), vbat(0.0), battery_numReadings(10) {
        client = cli;
        topic = top;
    }
    
    ~battery() {}
    
    float getVoltage(float reading) {   
        return reading * ((3.22 * 2) / 4095.0);
    }
    
    void begin() { 
        Serial.printf("Started Battery Level Monitor on pin %d\n", battery_inputPin);
        
        total = 0;
        vbat = 0.0;
        average = 0.0;  
        pinMode(A1, INPUT);
        analogReadResolution(12);
        
        //take the measurement before we start wifi...
    
        //throw away the first reading (garbage data)
        analogRead(battery_inputPin);
        delay(50);
    
        //take N samples
        for (int thisReading = 0; thisReading < battery_numReadings; thisReading++) {
            int my_reading = analogRead(battery_inputPin);
            total += my_reading;
        }
    
        //calc average reading
        average = (float) total / (float) battery_numReadings; 
    
        //convert avg to a voltage and store it for later reporting
        vbat = getVoltage(average);
        Serial.printf("(%dms) Battery Level: %f Volts\n", millis(), vbat);
    }
    
    void handle() {
        //print voltage generated in begin() 
        Serial.printf("(%dms) Battery Level: %f Volts\n", millis(), vbat);
    
        JsonDocument myObject;
        String myMsg;
    
        myObject["battery"] = vbat;
    
        tx_queue->enqueue(topic.c_str(), myObject);
    }
};

#endif