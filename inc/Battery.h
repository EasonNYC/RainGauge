#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"

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
    /**
     * @brief Constructs a battery monitor with ADC pin and MQTT integration
     * @param pin GPIO pin number for ADC battery voltage measurement
     * @param cli Pointer to PubSubClient for MQTT communication
     * @param q Pointer to MqttMessageQueue for message buffering
     * @param top MQTT topic string for battery data publication
     * 
     * Initializes the battery monitoring system with hardware and communication setup:
     * - Sets ADC input pin for voltage measurement
     * - Configures MQTT client and message queue references
     * - Initializes averaging variables (10-sample default)
     * - Resets total, average, and voltage storage
     * 
     * Default Configuration:
     * - 10 ADC samples for statistical averaging
     * - All measurement variables initialized to zero
     * - Ready for begin() method to perform actual sampling
     * 
     * The pin should be connected to a voltage divider network that scales
     * the battery voltage (typically 3.0-4.2V) to the ESP32 ADC range (0-3.3V).
     * 
     * MQTT message format: {"battery": voltage_in_volts}
     */
    battery(uint8_t pin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
        : battery_inputPin(pin), tx_queue(q), total(0), average(0.0), vbat(0.0), battery_numReadings(10) {
        client = cli;
        topic = top;
    }
    
    /**
     * @brief Destructor for battery monitor cleanup
     * 
     * Currently performs no specific cleanup operations as the ADC and GPIO
     * resources are managed automatically by the ESP32 system. All measurement
     * data is stored in stack variables that are automatically deallocated.
     * 
     * In future implementations, this could include:
     * - ADC power-down commands for ultra-low power operation
     * - GPIO pin reconfiguration to high-impedance state
     * - Statistical data logging or persistence operations
     * - Hardware power management cleanup
     */
    ~battery() {}
    
    /**
     * @brief Converts ADC reading to actual battery voltage
     * @param reading Raw ADC value (0-4095 for 12-bit resolution)
     * @return Battery voltage in volts after voltage divider compensation
     * 
     * Performs the conversion from raw ADC counts to actual battery voltage
     * by accounting for the voltage divider and ADC reference voltage.
     * 
     * Conversion Formula:
     * voltage = reading × (3.22V × 2) ÷ 4095
     * 
     * Circuit Analysis:
     * - 3.22V: ESP32 ADC reference voltage (calibrated value)
     * - 2: Voltage divider multiplication factor (2:1 ratio)
     * - 4095: Maximum ADC count for 12-bit resolution (2^12 - 1)
     * 
     * Hardware Requirements:
     * - Voltage divider with 2:1 ratio (e.g., two equal resistors)
     * - Battery+ → R1 → ADC_pin → R2 → GND
     * - This allows measuring up to ~6.4V battery voltage safely
     * 
     * Accuracy Notes:
     * - ADC reference may vary between ESP32 units
     * - Consider calibration for high-precision applications
     * - Temperature affects ADC accuracy (±3% typical)
     */
    float getVoltage(float reading) {   
        return reading * ((3.22 * 2) / 4095.0);
    }
    
    /**
     * @brief Initializes ADC and performs battery voltage sampling
     * 
     * Performs the complete battery voltage measurement process before WiFi
     * activation to avoid ADC interference. This method must be called during
     * setup before any WiFi operations.
     * 
     * Critical Timing:
     * - Must execute before WiFi.begin() or WiFi operations
     * - ESP32 ADC is affected by WiFi radio interference
     * - Measured voltage is stored for later reporting in handle()
     * 
     * Measurement Process:
     * 1. Configures GPIO pin as analog input
     * 2. Sets 12-bit ADC resolution (4095 maximum count)
     * 3. Discards first ADC reading (eliminates startup noise)
     * 4. Takes multiple samples (default: 10) for statistical averaging
     * 5. Calculates average ADC reading
     * 6. Converts to actual voltage using voltage divider compensation
     * 7. Stores result for MQTT transmission
     * 
     * Noise Reduction:
     * - First reading discarded (ADC settling time)
     * - 50ms delay after initial reading for stabilization
     * - Multi-sample averaging reduces random noise
     * - Statistical approach improves measurement accuracy
     * 
     * Serial Output:
     * - Initialization confirmation with pin number
     * - Final voltage reading with timestamp for verification
     * 
     * Hardware Setup Required:
     * - Voltage divider connected to specified pin
     * - Proper battery connection to voltage divider input
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
     * Transmits the battery voltage measured during begin() via both serial
     * output and MQTT message queue. This separation allows ADC measurement
     * before WiFi activation while reporting after network connectivity.
     * 
     * Reporting Process:
     * 1. Prints stored voltage to serial with timestamp
     * 2. Creates JSON document with battery voltage
     * 3. Queues MQTT message for network transmission
     * 
     * Data Sources:
     * - Uses voltage stored in vbat variable from begin()
     * - No new ADC measurements (avoids WiFi interference)
     * - Represents battery state at system startup
     * 
     * MQTT Message Format:
     * {"battery": voltage_in_volts}
     * 
     * Serial Output Format:
     * "({timestamp}ms) Battery Level: {voltage} Volts"
     * 
     * Usage Timing:
     * - Call after WiFi connection is established
     * - Safe to call multiple times (reports same stored value)
     * - Typically called during periodic sensor reporting cycles
     * 
     * Battery Monitoring Applications:
     * - Remote battery level monitoring
     * - Low battery alerts and warnings
     * - Power management decision making
     * - Predictive maintenance scheduling
     */
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