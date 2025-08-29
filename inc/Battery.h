#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"
#include "inc/BaseSensor.h"

// RTC persistent timing for battery sensor
RTC_DATA_ATTR unsigned long batteryLastUpdate = 0;

/* NOTE: The ADC doesnt work while WiFi is on. So the sampling happens in begin() and the reporting happens in the handle() function.
*/

/**
 * @brief Battery voltage monitoring system with averaged ADC sampling
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * 
 * This template class provides battery voltage monitoring for ESP32-based IoT devices
 * using analog-to-digital conversion with statistical averaging for accuracy. Features:
 * 
 * - Multi-sample averaging (default 10 samples) for noise reduction
 * - Voltage divider compensation for battery levels above ADC reference
 * - Pre-WiFi sampling to avoid ADC interference from radio operations
 * - MQTT integration for remote battery monitoring
 * - Serial debug output with timestamps
 * 
 * Hardware Configuration:
 * - Uses voltage divider (2:1 ratio) for battery measurement
 * - 12-bit ADC resolution (4095 levels) for precision
 * - 3.22V reference voltage compensation
 * - Typically connected to A1/GPIO pin for battery input
 * 
 * ADC Limitation:
 * - ESP32 ADC is affected by WiFi radio interference
 * - Sampling occurs in begin() before WiFi activation
 * - Stored voltage is reported later in handle() after WiFi is active
 * 
 * Power Management:
 * - Essential for battery-powered IoT devices
 * - Enables low-battery warnings and shutdown protection
 * - Supports predictive maintenance scheduling
 * 
 * Voltage Range: Designed for 3.0V - 4.2V Li-ion/LiPo battery monitoring
 */
template<size_t QUEUE_SIZE>
class battery : public BaseSensor {
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
    /**
     * @brief Constructs a battery monitor with ADC pin and MQTT integration
     * @param pin GPIO pin number for ADC battery voltage measurement
     * @param cli Pointer to PubSubClient for MQTT communication
     * @param q Pointer to MqttMessageQueue for message buffering
     * @param top MQTT topic string for battery data publication
     * 
     * Initializes battery monitoring with 10-sample averaging, MQTT integration,
     * and voltage storage. Pin should connect to voltage divider scaling battery
     * voltage (3.0-4.2V) to ESP32 ADC range (0-3.3V).
     * 
     * MQTT format: {"battery": voltage_in_volts}
     */
    battery(uint8_t pin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
        : battery_inputPin(pin), tx_queue(q), total(0), average(0.0), vbat(0.0), battery_numReadings(10) {
        client = cli;
        topic = top;
    }
    
    /**
     * @brief Destructor for battery monitor cleanup
     * 
     * Currently no specific cleanup as ADC/GPIO resources are managed
     * automatically by ESP32 system. Future enhancements could include
     * ADC power-down and GPIO reconfiguration for ultra-low power.
     */
    ~battery() {}
    
    /**
     * @brief Converts ADC reading to actual battery voltage
     * @param reading Raw ADC value (0-4095 for 12-bit resolution)
     * @return Battery voltage in volts after voltage divider compensation
     * 
     * Formula: voltage = reading × (3.22V × 2) ÷ 4095
     * - 3.22V: ESP32 ADC reference voltage
     * - 2: Voltage divider ratio (allows up to ~6.4V measurement)
     * - 4095: 12-bit ADC maximum count
     * 
     * Requires 2:1 voltage divider: Battery+ → R1 → ADC_pin → R2 → GND
     */
    float getVoltage(float reading) {   
        return reading * ((3.22 * 2) / 4095.0);
    }
    
    /**
     * @brief Initializes ADC and performs battery voltage sampling
     * 
     * CRITICAL: Must execute before WiFi operations to avoid ADC interference.
     * 
     * Process: Configures 12-bit ADC, discards first reading, takes 10 samples
     * for averaging, converts to voltage, and stores for later MQTT reporting.
     * 
     * Noise reduction: 50ms settling delay, multi-sample statistical averaging.
     * Serial output confirms initialization and final voltage measurement.
     * 
     * Requires voltage divider connected to specified pin.
     */
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
    
    /**
     * @brief Reports stored battery voltage via serial and MQTT
     * 
     * Transmits voltage measured during begin() after WiFi is active.
     * Uses stored value to avoid ADC/WiFi interference.
     * 
     * Process: Prints voltage with timestamp, creates JSON message,
     * queues for MQTT transmission.
     * 
     * Format: {"battery": voltage_in_volts}
     * Safe to call multiple times - reports same stored startup value.
     */
    void handle() {
        //print voltage generated in begin() 
        Serial.printf("(%dms) Battery Level: %f Volts\n", millis(), vbat);
    
        JsonDocument myObject;
        String myMsg;
    
        myObject["battery"] = vbat;
    
        tx_queue->enqueue(topic.c_str(), myObject);
    }

    // BaseSensor interface implementation
    unsigned long getUpdateInterval() override {
        return 300000; // 5 minutes for battery monitoring
    }
    
    bool needsUpdate() override {
        return false; // Battery is not time-critical, only scheduled updates
    }
    
    String getSensorId() override {
        return "Battery";
    }
};

#endif