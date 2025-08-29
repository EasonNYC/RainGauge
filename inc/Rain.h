
#ifndef RAIN_H
#define RAIN_H

#include "Arduino.h"
#include <ArduinoJson.h>
#include <FunctionalInterrupt.h>
#include "inc/MqttMessageQueue.h"
#include "inc/BaseSensor.h"

// Forward declarations
class PubSubClient;


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

//persistent data
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int latest_Raincount = 0;
RTC_DATA_ATTR bool activeRain = false;

// Sensor scheduler persistent timing data
RTC_DATA_ATTR unsigned long schedulerLastWakeTime = 0;
RTC_DATA_ATTR unsigned long schedulerSleepDuration = 0;
RTC_DATA_ATTR unsigned long batteryLastUpdate = 0;
RTC_DATA_ATTR unsigned long rainGaugeLastUpdate = 0;
RTC_DATA_ATTR unsigned long soilTempLastUpdate = 0;
RTC_DATA_ATTR unsigned long bmp280LastUpdate = 0;

float unit_of_rain = 0.01193;//inches per pulse

/**
 * @brief Tipping bucket rain gauge interface with interrupt-driven measurement
 * @tparam QUEUE_SIZE The size of the MQTT message queue
 * 
 * This template class provides a complete interface for tipping bucket rain gauges
 * with interrupt-based rain detection and MQTT integration. Features:
 * 
 * - Hardware interrupt-driven rain detection with debouncing
 * - RTC persistent data storage for deep sleep applications  
 * - Automatic rainfall accumulation and hourly reporting
 * - MQTT message queuing for reliable data transmission
 * - Debounced bucket tip detection (100ms minimum interval)
 * - Calibrated rainfall measurement (0.01193 inches per tip)
 * 
 * Uses RTC_DATA_ATTR variables to maintain rain counts across ESP32 deep sleep cycles.
 * Handles both active rain detection and scheduled periodic updates.
 */
template<size_t QUEUE_SIZE>
class Raingauge : public BaseSensor {
  
public:
  /**
   * @brief Constructs a rain gauge instance with hardware and MQTT configuration
   * @param reqPin GPIO pin number connected to the rain gauge tipping bucket sensor
   * @param cli Pointer to PubSubClient for MQTT communication
   * @param q Pointer to MqttMessageQueue for message buffering
   * @param top MQTT topic string for rainfall data publication
   * 
   * Configures GPIO as INPUT_PULLUP for tipping bucket, sets up MQTT
   * integration, initializes timing for interrupt debouncing.
   * Pin connects to normally-closed bucket that pulls LOW on tip.
   */
  Raingauge(uint8_t reqPin, PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top) 
  : PIN(reqPin),client(cli),tx_queue(q),topic(top)
  {
    
    pinMode(PIN, INPUT_PULLUP);
    _lastMillis = millis();
  };

  /**
   * @brief Initializes interrupt-based rain detection and begins operation
   * 
   * Attaches hardware interrupt to GPIO pin for tipping bucket detection.
   * FALLING edge trigger when bucket tips and pulls pin LOW.
   * 
   * Call once during setup. Interrupt active until destructor called.
   * Prints initialization confirmation to serial.
   */
  void begin(){
    attachInterrupt(PIN, std::bind(&Raingauge::isr,this), FALLING);
    Serial.printf("Started Raingauge on pin %d\n", PIN);
  }

  /**
   * @brief Destructor that cleanly shuts down interrupt handling
   * 
   * Detaches hardware interrupt to prevent spurious interrupts and crashes
   * after object destruction. Called automatically on scope exit.
   */
  ~Raingauge(){
    detachInterrupt(PIN);
  }

  /**
   * @brief Hardware interrupt service routine for tipping bucket detection
   * 
   * Called when bucket tips and pulls pin LOW. 100ms debouncing prevents
   * mechanical bounce while allowing rapid rain detection.
   * 
   * Process: Check debounce time, increment counters, update RTC data,
   * set rain flags, update timestamp.
   * 
   * WARNING: Keep minimal - no Serial.print, delays, or complex operations.
   */
  void ARDUINO_ISR_ATTR isr(){
    if(millis()-_lastMillis > 100) {
      _rainBucketsDumped += 1;
      latest_Raincount += _rainBucketsDumped;
      activeRain = true;
      _lastMillis = millis();
    }
  }


  /**
   * @brief Checks if active rainfall has been detected
   * @return true if rain has been detected since last update, false otherwise
   * 
   * Returns global activeRain flag set by ISR on bucket tips.
   * Persists across deep sleep via RTC_DATA_ATTR storage.
   * Used for conditional MQTT reporting and main loop processing.
   */
  bool isRaining() { return activeRain;}

  /**
   * @brief Processes accumulated rainfall data and publishes via MQTT
   * 
   * Calculates rainfall using 0.01193"/tip, creates MQTT message, resets
   * counters for next period. Called during scheduled updates.
   * 
   * Process: Calculate inches, print debug if rain detected, create JSON,
   * queue for MQTT, reset all counters/flags.
   * 
   * Format: {"rain": rainfall_in_inches}. Sends 0.0 if no rain.
   */
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
  }

  /**
   * @brief Prints detailed rainfall statistics to serial console for debugging
   * 
   * Outputs raw tip counts and calculated inches with timestamps.
   * Format: "Detected rain {count} times" and "LastHour: {amount} inches"
   * 
   * Helps verify sensor operation, calibration, and timing during development.
   */
  void reportRain(){
      float rainLastHour = (float)latest_Raincount*unit_of_rain;
      Serial.printf("(%dms) Rainfall Report: Detected rain %u times in the last hour\n", millis(), latest_Raincount);
      Serial.printf("(%dms) Rainfall Report: LastHour: %f inches\n", millis(), rainLastHour);
  }

  /**
   * @brief Main processing method for scheduled rainfall updates
   * 
   * Checks timer wakeup flag, processes complete measurement cycle when due.
   * Call regularly from main loop.
   * 
   * Process: Check isTimeToUpdate(), if due call reportRain() for debug
   * and updateRain() for processing/transmission.
   * 
   * Main interface for automatic measurement/reporting workflow.
   */
  void handle() {
    if(isTimeToUpdate()){
        reportRain();
        updateRain();
    }
  }

  // BaseSensor interface implementation
  unsigned long getUpdateInterval() override {
    return 60000; // 60 seconds for rain gauge
  }
  
  bool needsUpdate() override {
    return false; // Only scheduled updates via SensorScheduler
  }
  
  String getSensorId() override {
    return "RainGauge";
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

#endif