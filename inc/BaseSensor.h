#ifndef BASESENSOR_H
#define BASESENSOR_H

#include "Arduino.h"

/**
 * @brief Abstract base class for all sensor types in the weather station
 * 
 * Provides common interface for sensor management, scheduling, and lifecycle.
 * All sensors implement begin(), handle(), and timing methods for unified
 * management by SensorScheduler.
 */
class BaseSensor {
public:
    /**
     * @brief Initialize the sensor hardware and configuration
     * 
     * Called once during system startup. Should configure GPIO pins,
     * initialize communication protocols, and prepare sensor for operation.
     */
    virtual void begin() = 0;
    
    /**
     * @brief Process sensor reading and MQTT transmission
     * 
     * Called when sensor update is due. Should read data, format for MQTT,
     * and queue message for transmission. Reset any timing flags.
     */
    virtual void handle() = 0;
    
    /**
     * @brief Get sensor's update interval in milliseconds
     * @return Update interval in milliseconds
     * 
     * Returns how often this sensor should be read/updated.
     * Used by SensorScheduler to determine wake times.
     */
    virtual unsigned long getUpdateInterval() = 0;
    
    /**
     * @brief Check if sensor needs an update right now
     * @return true if sensor should be updated, false otherwise
     * 
     * Used for immediate checks without waiting for scheduled time.
     * Useful for interrupt-driven sensors or immediate data needs.
     */
    virtual bool needsUpdate() = 0;
    
    /**
     * @brief Get sensor's unique identifier
     * @return String identifier for this sensor
     * 
     * Used for debugging, logging, and sensor management.
     * Should be unique across all sensors in the system.
     */
    virtual String getSensorId() = 0;
    
    /**
     * @brief Get pointer to sensor's RTC persistent timing variable
     * @return Pointer to RTC_DATA_ATTR variable storing last update time
     * 
     * Each sensor manages its own RTC persistent timing data.
     * Used by SensorScheduler for timing calculations across deep sleep.
     */
    virtual unsigned long* getLastUpdatePtr() = 0;
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~BaseSensor() {}
};

#endif