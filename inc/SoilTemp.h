
#ifndef SOILTEMP_H
#define SOILTEMP_H

#include <OneWire.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"

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
class Tempsensor{
  
public:
  /**
   * @brief Constructs a temperature sensor instance
   * @param pin The GPIO pin number for OneWire communication (requires 4.7K pull-up)
   * @param cli Pointer to PubSubClient for MQTT communication
   * @param q Pointer to MqttMessageQueue for message buffering
   * @param top MQTT topic string for temperature data publication
   * 
   * Initializes the DS18B20 temperature sensor on the specified pin with MQTT
   * integration. The sensor will publish temperature readings to the provided
   * topic via the message queue system.
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
   * Performs sensor discovery on the OneWire bus and starts the first temperature
   * conversion. This method should be called once during setup after construction.
   * 
   * Process:
   * 1. Searches for DS18B20 device on the OneWire bus
   * 2. Stores the device address for future communications
   * 3. Initiates the first temperature conversion
   * 
   * If no sensor is found, prints error message and resets the search.
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
   * @brief Initiates a non-blocking temperature conversion
   * 
   * Starts the DS18B20 temperature conversion process without blocking execution.
   * The conversion takes up to 750ms (for 12-bit resolution) to complete.
   * 
   * Process:
   * 1. Resets the OneWire bus
   * 2. Selects the target sensor using stored address
   * 3. Sends 0x44 command (Convert T) with parasite power enabled
   * 4. Records start time and sets sensor state to converting (1)
   * 
   * Call waitForDataReady() to check conversion completion.
   */
  void startConversion(){

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
    convMillis = millis();
    sensorState = 1;

  }
  
  /**
   * @brief Checks if temperature conversion is complete (non-blocking)
   * 
   * Monitors the elapsed time since startConversion() was called and updates
   * the sensor state when conversion is complete. Uses a 1000ms timeout which
   * provides margin above the maximum 750ms conversion time for 12-bit resolution.
   * 
   * State transitions:
   * - sensorState = 1: Conversion in progress
   * - sensorState = 2: Conversion complete, data ready to read
   * 
   * Call this method periodically in the main loop after startConversion().
   */
  void waitForDataReady(){
    if((millis() - convMillis) > 1000) {
      sensorState = 2;
    }

  }

  /**
   * @brief Reads temperature data from the DS18B20 sensor
   * 
   * Retrieves the 9-byte scratchpad data from the DS18B20 after temperature
   * conversion is complete. This data contains the raw temperature value and
   * configuration information needed for temperature calculation.
   * 
   * Process:
   * 1. Resets the OneWire bus and selects the sensor
   * 2. Sends 0xBE command (Read Scratchpad)
   * 3. Reads all 9 bytes of scratchpad data into internal buffer
   * 4. Resets sensor state to idle (0)
   * 
   * Call only after waitForDataReady() indicates conversion is complete.
   * Use getC() or getF() to calculate temperature from the raw data.
   */
  void readData(){
    ds.reset();             //present
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad
 
    for (int i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
      //  WebSerial.print(data[i], HEX);
      //  WebSerial.print(" ");
    }
    sensorState = 0;
  }

  /**
   * @brief Calculates temperature in Celsius from raw sensor data
   * @return Temperature value in degrees Celsius
   * 
   * Converts the raw 16-bit temperature data from the DS18B20 scratchpad into
   * a floating-point Celsius temperature. Handles different resolution modes:
   * 
   * Resolution modes:
   * - 9-bit:  0.5°C resolution, 93.75ms conversion time
   * - 10-bit: 0.25°C resolution, 187.5ms conversion time  
   * - 11-bit: 0.125°C resolution, 375ms conversion time
   * - 12-bit: 0.0625°C resolution, 750ms conversion time (default)
   * 
   * The method handles both DS18S20 (type_s = true) and DS18B20 (type_s = false)
   * sensor types with appropriate bit masking for the configured resolution.
   * 
   * Call only after readData() has retrieved fresh sensor data.
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
   * Converts the sensor temperature to Fahrenheit by first getting the Celsius
   * reading from getC() and applying the standard conversion formula:
   * °F = (°C × 1.8) + 32
   * 
   * This is a convenience method commonly used in applications where Fahrenheit
   * is the preferred temperature scale. The conversion maintains the same
   * precision as the underlying Celsius reading.
   * 
   * Call only after readData() has retrieved fresh sensor data.
   */
  float getF(){
    float celcius = getC();
    return celcius * 1.8 + 32.0;
  }

  /**
   * @brief Handles complete temperature reading and MQTT publishing cycle
   * 
   * Performs the complete temperature measurement workflow when called:
   * 1. Reads the raw temperature data from the sensor
   * 2. Reports the temperature to serial console (debug output)
   * 3. Creates JSON message with temperature in Fahrenheit
   * 4. Queues the message for MQTT transmission
   * 
   * This is typically called after waitForDataReady() indicates that a
   * temperature conversion is complete. The method automatically handles
   * all steps from data retrieval to message queuing.
   * 
   * The MQTT message format: {"soil_temp": temperature_in_fahrenheit}
   */
  void handle(){

    readData();
    reportF();

    JsonDocument myObject;

    myObject["soil_temp"] = getF();

    tx_queue->enqueue(topic.c_str(), myObject);

  }
    
  /**
   * @brief Prints current temperature reading to serial console for debugging
   * 
   * Outputs a formatted temperature reading in Fahrenheit to the serial port
   * with a timestamp. Format: "({timestamp}ms) Soil Temp = {temperature}F"
   * 
   * This debug output helps monitor sensor operation and verify temperature
   * readings during development and troubleshooting. The timestamp shows
   * milliseconds since boot for correlation with other system events.
   */
  void reportF(){
    Serial.printf("(%dms) Soil Temp = %fF\n", millis(), getF());
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
    int sensorState = 0;
    volatile unsigned long convMillis = 0;
    
};

#endif