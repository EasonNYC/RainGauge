
#ifndef RAIN_H
#define RAIN_H

#include "Arduino.h"
#include <ArduinoJson.h>
#include <FunctionalInterrupt.h>
#include "inc/MqttMessageQueue.h"

// Forward declarations
class PubSubClient;


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

//persistent data
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int latest_Raincount = 0;
RTC_DATA_ATTR bool activeRain = false;
RTC_DATA_ATTR bool timeToUpdate = false;

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
class Raingauge {
  
public:
  /**
   * @brief Constructs a rain gauge instance with hardware and MQTT configuration
   * @param reqPin GPIO pin number connected to the rain gauge tipping bucket sensor
   * @param cli Pointer to PubSubClient for MQTT communication
   * @param q Pointer to MqttMessageQueue for message buffering
   * @param top MQTT topic string for rainfall data publication
   * 
   * Initializes the rain gauge hardware interface and MQTT integration:
   * - Configures the specified GPIO pin as INPUT_PULLUP for the tipping bucket sensor
   * - Sets up MQTT client and message queue references
   * - Initializes timing variables for interrupt debouncing
   * 
   * The pin should be connected to a normally-closed tipping bucket mechanism
   * that pulls the pin LOW when a tip occurs (hence INPUT_PULLUP configuration).
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
   * Attaches a hardware interrupt to the configured GPIO pin to detect tipping
   * bucket events. The interrupt triggers on FALLING edge when the bucket tips
   * and pulls the pin LOW.
   * 
   * Process:
   * 1. Attaches interrupt handler (isr method) to the GPIO pin
   * 2. Configures FALLING edge trigger for tip detection
   * 3. Prints initialization confirmation to serial
   * 
   * Call this method once during setup after construction to enable rain detection.
   * The interrupt will remain active until the destructor is called.
   */
  void begin(){
    attachInterrupt(PIN, std::bind(&Raingauge::isr,this), FALLING);
    Serial.printf("Started Raingauge on pin %d\n", PIN);
  }

  /**
   * @brief Destructor that cleanly shuts down interrupt handling
   * 
   * Detaches the hardware interrupt from the GPIO pin to prevent spurious
   * interrupts after the object is destroyed. This ensures clean shutdown
   * and prevents potential crashes or undefined behavior.
   * 
   * Automatically called when the object goes out of scope or is explicitly deleted.
   */
  ~Raingauge(){
    detachInterrupt(PIN);
  }

  /**
   * @brief Hardware interrupt service routine for tipping bucket detection
   * 
   * This ISR is automatically called when the tipping bucket pulls the GPIO pin LOW.
   * Implements debouncing to prevent multiple counts from mechanical bounce or
   * rapid successive tips.
   * 
   * Process:
   * 1. Checks if 100ms has elapsed since last tip (debouncing)
   * 2. Increments local bucket dump counter
   * 3. Updates global rain count (RTC persistent data)
   * 4. Sets active rain flag for status reporting
   * 5. Updates last tip timestamp
   * 
   * The 100ms debounce prevents false triggers from mechanical bounce while
   * still allowing detection of rapid legitimate rainfall events.
   * 
   * WARNING: Keep ISR code minimal - avoid Serial.print, delays, or complex operations.
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
   * @brief Checks if it's time for a scheduled rainfall update
   * @return true if a periodic update is due, false otherwise
   * 
   * Returns the state of the global timeToUpdate flag, which is set by the
   * ESP32 timer wakeup mechanism. This flag indicates that the scheduled
   * reporting interval has elapsed and rainfall data should be transmitted.
   * 
   * Used by the main loop to determine when to process and send accumulated
   * rainfall measurements via MQTT.
   */
  bool isTimeToUpdate() { return timeToUpdate;}

  /**
   * @brief Checks if active rainfall has been detected
   * @return true if rain has been detected since last update, false otherwise
   * 
   * Returns the state of the global activeRain flag, which is set by the
   * interrupt service routine when bucket tips are detected. This flag
   * persists across deep sleep cycles using RTC_DATA_ATTR storage.
   * 
   * Used to determine if rainfall data needs to be included in MQTT reports
   * and for conditional processing in the main application loop.
   */
  bool isRaining() { return activeRain;}

  /**
   * @brief Processes accumulated rainfall data and publishes via MQTT
   * 
   * Calculates total rainfall from accumulated bucket tips, creates an MQTT message,
   * and resets counters for the next measurement period. This method is typically
   * called during scheduled updates (hourly intervals).
   * 
   * Process:
   * 1. Calculates rainfall in inches using calibration factor (0.01193"/tip)
   * 2. If rain detected, prints debug information and resets active rain flag
   * 3. Creates JSON message with rainfall amount
   * 4. Queues message for MQTT transmission
   * 5. Resets all counters and flags for next measurement period
   * 
   * Message format: {"rain": rainfall_in_inches}
   * 
   * Even if no rain detected, sends a message with rain=0.0 to confirm system operation.
   * Resets global RTC persistent variables to prepare for next measurement cycle.
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
    timeToUpdate = false;
  }

  /**
   * @brief Prints detailed rainfall statistics to serial console for debugging
   * 
   * Outputs comprehensive rainfall information including both raw bucket tip counts
   * and calculated rainfall amounts in inches. Includes timestamps for correlation
   * with other system events.
   * 
   * Output format:
   * - Raw tip count: "Detected rain {count} times in the last hour"  
   * - Calculated rainfall: "LastHour: {amount} inches"
   * - Both messages prefixed with millisecond timestamp
   * 
   * This debug output helps verify sensor operation, calibration accuracy, and
   * system timing during development and troubleshooting.
   */
  void reportRain(){
      float rainLastHour = (float)latest_Raincount*unit_of_rain;
      Serial.printf("(%dms) Rainfall Report: Detected rain %u times in the last hour\n", millis(), latest_Raincount);
      Serial.printf("(%dms) Rainfall Report: LastHour: %f inches\n", millis(), rainLastHour);
  }

  /**
   * @brief Main processing method for scheduled rainfall updates
   * 
   * Checks if a scheduled update is due and processes the complete rainfall
   * measurement cycle when needed. This method should be called regularly
   * from the main application loop.
   * 
   * Process:
   * 1. Checks isTimeToUpdate() flag set by timer wakeup
   * 2. If update is due:
   *    - Calls reportRain() for debug output
   *    - Calls updateRain() to process and transmit data
   * 
   * This provides the main interface between the application loop and the
   * rain gauge functionality, handling the complete measurement and reporting
   * workflow automatically when scheduled intervals occur.
   */
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

#endif