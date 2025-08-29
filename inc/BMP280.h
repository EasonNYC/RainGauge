#ifndef BMP280_H
#define BMP280_H

#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include "inc/MqttMessageQueue.h"
#include "inc/BaseSensor.h"

// Forward declarations
class PubSubClient;

Adafruit_BMP280 bmp; // I2C

/**
 * @brief BMP280 temperature and pressure sensor interface with MQTT integration
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * 
 * This template class provides a complete interface for the Bosch BMP280 
 * environmental sensor via I2C communication. Features:
 * 
 * - Temperature measurement with Celsius to Fahrenheit conversion
 * - Barometric pressure measurement in Pascals
 * - Forced measurement mode for power-efficient operation
 * - Configurable oversampling and filtering for accuracy vs. speed
 * - Automatic MQTT message queuing for reliable data transmission
 * - Serial debug output with timestamps for monitoring
 * 
 * Sensor Specifications:
 * - Temperature range: -40°C to +85°C (±1°C accuracy)
 * - Pressure range: 300-1100 hPa (±1 hPa accuracy)
 * - I2C interface with configurable address
 * - Ultra-low power consumption in forced mode
 * 
 * Configuration:
 * - MODE_FORCED: Single measurement then sleep (power efficient)
 * - 2x temperature oversampling, 16x pressure oversampling
 * - 16x digital filtering for noise reduction
 * - 500ms standby time between measurements
 * 
 * Essential for weather monitoring, altitude sensing, and environmental data logging
 * in battery-powered IoT applications.
 */
template<size_t QUEUE_SIZE>
class bmp280sensor : public BaseSensor {

  PubSubClient* client;
  MqttMessageQueue<QUEUE_SIZE>* tx_queue;
  String topic;

  public:
  /**
   * @brief Constructs a BMP280 sensor instance with MQTT integration
   * @param cli Pointer to PubSubClient for MQTT communication
   * @param q Pointer to MqttMessageQueue for message buffering
   * @param top MQTT topic string for sensor data publication
   * 
   * Initializes BMP280 interface with MQTT connectivity. Hardware
   * initialization occurs in begin() method.
   * 
   * MQTT format: {"bmp_temperature": temp_f, "bmp_pressure": pressure_pa}
   */
  bmp280sensor(PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
  :client(cli),tx_queue(q),topic(top)
  {

  }
  
  /**
   * @brief Destructor for BMP280 sensor cleanup
   * 
   * No specific cleanup as Adafruit_BMP280 library handles I2C resources
   * automatically. Future enhancements could include sensor power-down
   * commands and I2C bus cleanup for ultra-low power operation.
   */
  ~bmp280sensor(){}

  /**
   * @brief Initializes the BMP280 sensor with I2C communication and configuration
   * 
   * Establishes communication with the BMP280 sensor and applies optimized
   * settings for weather monitoring applications. This method must be called
   * once during setup before attempting measurements.
   * 
   * Initialization Process:
   * 1. Attempts I2C communication using default address (0x77)
   * 2. Validates sensor presence and chip ID
   * 3. Applies power-efficient configuration settings
   * 4. Confirms successful initialization via serial output
   * 
   * Configuration Applied:
   * - MODE_FORCED: Single measurement then sleep (ultra-low power)
   * - Temperature: 2x oversampling (±0.5°C accuracy, 16-bit resolution)
   * - Pressure: 16x oversampling (±0.12 hPa accuracy, 20-bit resolution)
   * - Digital Filter: 16x filtering (reduces noise from environmental vibration)
   * - Standby Time: 500ms between measurements in normal mode
   * 
   * Error Handling:
   * - Enters infinite loop with error message if sensor not found
   * - Check I2C wiring, power supply, and address conflicts if initialization fails
   * 
   * Alternative address (0x76) configuration is commented but available for
   * sensors with different address pin configuration.
   */
  void begin() {

     //if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
    if (!bmp.begin()) {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
        while (1) delay(10);
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    Serial.printf("Started bmp280 via I2C\n");
  }

  /**
   * @brief Converts Celsius temperature to Fahrenheit
   * @param celcius Temperature value in degrees Celsius
   * @return Temperature value in degrees Fahrenheit
   * 
   * Standard conversion: °F = (°C × 1.8) + 32
   * Maintains sensor precision in converted result.
   * Example: 25.0°C → 77.0°F
   */
  float getF(float celcius){
    return celcius * 1.8 + 32.0;
  }

  /**
   * @brief Performs a complete measurement cycle and publishes data via MQTT
   * 
   * Executes a full environmental sensing workflow including measurement,
   * data processing, debug output, and MQTT message queuing. This method
   * should be called when environmental data is needed.
   * 
   * Measurement Process:
   * 1. Triggers forced measurement mode (sensor wakes, measures, sleeps)
   * 2. Reads raw temperature and pressure from sensor registers
   * 3. Converts temperature from Celsius to Fahrenheit
   * 4. Outputs formatted readings to serial with timestamps
   * 5. Creates JSON message with both temperature and pressure
   * 6. Queues message for MQTT transmission
   * 
   * Power Efficiency:
   * - Uses forced measurement mode for minimum power consumption
   * - Sensor automatically returns to sleep after measurement
   * - Ideal for battery-powered periodic sensing applications
   * 
   * Data Format:
   * - Temperature: Degrees Fahrenheit (converted from sensor's Celsius)
   * - Pressure: Pascals (sensor's native unit)
   * - MQTT JSON: {"bmp_temperature": temp_f, "bmp_pressure": pressure_pa}
   * 
   * Error Handling:
   * - Prints error message if forced measurement fails
   * - Still attempts to queue data (may contain stale values)
   * - Check I2C connectivity if measurement failures persist
   * 
   * Serial Output Format:
   * - "({timestamp}ms) Outdoor Temperature = {temp} *F"
   * - "({timestamp}ms) Outdoor Pressure = {pressure} Pa"
   */
  void handle(){

    float temperature;
    float pressure;

    //get data
    if (bmp.takeForcedMeasurement()) {

        // can now print out the new measurements
        temperature = getF(bmp.readTemperature());
        pressure = bmp.readPressure();

        //print to the screen
        Serial.printf("(%dms) Outdoor Temperature = %f *F\n", millis(), temperature);
        Serial.printf("(%dms) Outdoor Pressure = %f Pa\n",  millis(), pressure);

    } else {
        Serial.println("BMP Forced measurement failed!");
    }

    JsonDocument myObject;

    myObject["bmp_temperature"] = temperature;
    myObject["bmp_pressure"] = pressure;

    tx_queue->enqueue(topic.c_str(), myObject);
  }

  // BaseSensor interface implementation
  unsigned long getUpdateInterval() override {
    return 180000; // 3 minutes for atmospheric sensors
  }
  
  bool needsUpdate() override {
    return false; // Scheduled updates only, not time-critical
  }
  
  String getSensorId() override {
    return "BMP280";
  }

};

#endif