
#ifndef SOILTEMP_H
#define SOILTEMP_H

#include <OneWire.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"
#include "inc/BaseSensor.h"

// RTC persistent timing for soil temperature sensor
RTC_DATA_ATTR unsigned long soilTempLastUpdate = 0;

// Forward declarations
class PubSubClient;

 // on pin 10 (a 4.7K resistor is necessary)

/**
 * @brief Dallas DS18B20 temperature sensor interface with MQTT integration
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * 
 * This template class provides a complete interface for Dallas DS18B20 OneWire
 * temperature sensors commonly used for soil temperature monitoring. Features:
 * 
 * - Non-blocking temperature conversion with state management
 * - Automatic sensor discovery and initialization
 * - Temperature readings in both Celsius and Fahrenheit
 * - MQTT message queuing for reliable data transmission
 * - Serial debug output for monitoring
 * 
 * Requires a 4.7K pull-up resistor on the OneWire data line.
 * Supports multiple sensor resolution modes (9-12 bit).
 */
template<size_t QUEUE_SIZE>
class Tempsensor : public BaseSensor {
  
public:
  /**
   * @brief Constructs a temperature sensor instance
   * @param pin The GPIO pin number for OneWire communication (requires 4.7K pull-up)
   * @param cli Pointer to PubSubClient for MQTT communication
   * @param q Pointer to MqttMessageQueue for message buffering
   * @param top MQTT topic string for temperature data publication
   * 
   * Initializes DS18B20 sensor with MQTT integration. Publishes readings
   * to specified topic via message queue system.
   */
  Tempsensor(uint8_t pin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
  :ds(pin),type_s(0),client(cli),tx_queue(q),topic(top)
  {
    saved_pin = pin;
  }

  ~Tempsensor(){
   
  }

  /**
   * @brief Initializes the temperature sensor and begins operation
   * 
   * Discovers DS18B20 on OneWire bus, stores address, starts first conversion.
   * Call once during setup after construction.
   * 
   * Prints error and resets search if no sensor found.
   */
  void begin(){

    Serial.printf("Started Soiltemp on pin %d\n", saved_pin);
    
    if ( !ds.search(addr)) {
        Serial.println("No more addresses.");
        ds.reset_search();
        delay(250);   
    }

    startConversion();

  }
  
  /**
   * @brief Initiates a temperature conversion
   * 
   * Starts DS18B20 conversion (up to 750ms for 12-bit). Resets bus,
   * selects sensor, sends 0x44 command.
   */
  void startConversion(){
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  }
  

  /**
   * @brief Reads temperature data from the DS18B20 sensor
   * 
   * Retrieves 9-byte scratchpad after conversion complete. Resets bus,
   * selects sensor, sends 0xBE command, reads data.
   * 
   * Call after conversion is complete. Use getC()/getF() for temperature.
   */
  void readData(){
    ds.reset();             //present
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad
 
    for (int i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
    }
  }

  /**
   * @brief Calculates temperature in Celsius from raw sensor data
   * @return Temperature value in degrees Celsius
   * 
   * Converts raw 16-bit data to Celsius. Handles resolution modes:
   * 9-bit (0.5°C), 10-bit (0.25°C), 11-bit (0.125°C), 12-bit (0.0625°C).
   * 
   * Supports DS18S20/DS18B20 with appropriate bit masking.
   * Call after readData() retrieves fresh data.
   */
  float getC(){
    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    float celsius = (float)raw / 16.0;
    return celsius;
  }

  /**
   * @brief Calculates temperature in Fahrenheit from raw sensor data  
   * @return Temperature value in degrees Fahrenheit
   * 
   * Gets Celsius from getC() and applies: °F = (°C × 1.8) + 32
   * Convenience method maintaining same precision as Celsius reading.
   * Call after readData() retrieves fresh data.
   */
  float getF(){
    float celcius = getC();
    return celcius * 1.8 + 32.0;
  }

  /**
   * @brief Handles complete temperature reading and MQTT publishing cycle
   * 
   * Complete workflow: reads data, reports to serial, creates JSON,
   * queues for MQTT. Call after waitForDataReady() shows complete.
   * 
   * Handles all steps from data retrieval to message queuing.
   * Format: {"soil_temp": temperature_in_fahrenheit}
   */
  void handle(){
    Serial.printf("(%dms) Starting soil temp conversion...\n", millis());
    
    // Use existing methods for conversion
    startConversion();
    delay(1000); // Wait for DS18B20 conversion (750ms + margin)
    readData();
    
    // Report and queue data
    reportF();

    JsonDocument myObject;
    myObject["soil_temp"] = getF();
    tx_queue->enqueue(topic.c_str(), myObject);
    
    Serial.printf("(%dms) SoilTemp queued for MQTT\n", millis());
  }
    
  /**
   * @brief Prints current temperature reading to serial console for debugging
   * 
   * Outputs Fahrenheit temperature with timestamp to serial.
   * Format: "({timestamp}ms) Soil Temp = {temperature}F"
   * 
   * Helps monitor sensor operation during development and troubleshooting.
   */
  void reportF(){
    Serial.printf("(%dms) Soil Temp = %fF\n", millis(), getF());
  }

  // BaseSensor interface implementation
  unsigned long getUpdateInterval() override {
    return 120000; // 2 minutes for soil temperature
  }
  
  bool needsUpdate() override {
    return false; // Scheduled updates only, no immediate needs
  }
  
  String getSensorId() override {
    return "SoilTemp";
  }
  
  unsigned long* getLastUpdatePtr() override {
    return &soilTempLastUpdate;
  }

private:
    //const uint8_t PIN;
    OneWire ds;
    int saved_pin;
    byte data[9];
    byte addr[8];
    byte type_s;
    PubSubClient* client;
    MqttMessageQueue<QUEUE_SIZE>* tx_queue;
    String topic;
    
};

#endif