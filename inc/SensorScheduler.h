#ifndef SENSORSCHEDULER_H
#define SENSORSCHEDULER_H

#include "Arduino.h"
#include "inc/BaseSensor.h"
#include <vector>
#include "esp_sleep.h"

// RTC persistent variables for scheduler timing
RTC_DATA_ATTR unsigned long schedulerLastWakeTime = 0;
RTC_DATA_ATTR unsigned long schedulerSleepDuration = 0;


/**
 * @brief Manages sensor update scheduling for ESP32 deep sleep cycles
 * 
 * Coordinates multiple sensors with different update intervals across deep sleep
 * wake cycles. Uses RTC persistent variables to track timing since millis() 
 * resets to 0 on each wake. Optimizes sleep duration based on sensor needs.
 */
class SensorScheduler {
private:
    /**
     * @brief Internal structure to track sensor timing and state
     */
    struct SensorTask {
        BaseSensor* sensor;         // Pointer to sensor instance
        unsigned long interval;     // Update interval in milliseconds
        unsigned long* lastUpdate;  // Pointer to RTC persistent last update time
        bool enabled;              // Whether this sensor is active
        
        SensorTask(BaseSensor* s, unsigned long inter, unsigned long* lastUpd) 
            : sensor(s), interval(inter), lastUpdate(lastUpd), enabled(true) {}
    };
    
    std::vector<SensorTask> tasks;
    unsigned long currentWakeTime;
    bool firstBoot;
    
public:
    /**
     * @brief Constructor initializes timing for current wake cycle
     */
    SensorScheduler() {
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        
        if (schedulerLastWakeTime == 0) {
            // First boot - set all sensors to run immediately
            currentWakeTime = millis();
            firstBoot = true;
            Serial.printf("First boot detected - currentWakeTime: %lu\n", currentWakeTime);
        } else {
            // Always advance by full intended duration regardless of wake reason
            // This ensures timer-based sensor scheduling isn't disrupted by rain interrupts
            currentWakeTime = schedulerLastWakeTime + schedulerSleepDuration;
            
            if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
                Serial.printf("Rain wake - treating as timer wake for scheduling: %lu (last: %lu + duration: %lu)\n", 
                             currentWakeTime, schedulerLastWakeTime, schedulerSleepDuration);
            } else {
                Serial.printf("Timer wake - currentWakeTime: %lu (last: %lu + duration: %lu)\n", 
                             currentWakeTime, schedulerLastWakeTime, schedulerSleepDuration);
            }
            firstBoot = false;
        }
    }
    
    /**
     * @brief Add a sensor to the scheduling system
     * @param sensor Pointer to sensor implementing BaseSensor interface
     * 
     * Registers sensor with scheduler using its getUpdateInterval().
     * Calls sensor->begin() to initialize hardware.
     * Each sensor manages its own RTC persistent timing data.
     */
    void addSensor(BaseSensor* sensor) {
        if (sensor == nullptr) return;
        
        sensor->begin();
        
        unsigned long* persistentLastUpdate = sensor->getLastUpdatePtr();
        if (persistentLastUpdate != nullptr) {
            SensorTask task(sensor, sensor->getUpdateInterval(), persistentLastUpdate);
            tasks.push_back(task);
            
            Serial.printf("Added sensor %s with %lu ms interval (lastUpdate: %lu)\n", 
                         sensor->getSensorId().c_str(), 
                         sensor->getUpdateInterval(),
                         *persistentLastUpdate);
        }
    }
    
    /**
     * @brief Remove a sensor from scheduling
     * @param sensorId String identifier of sensor to remove
     * 
     * Disables sensor in schedule without destroying the sensor object.
     * Useful for temporarily disabling problematic sensors.
     */
    void removeSensor(String sensorId) {
        for (auto& task : tasks) {
            if (task.sensor->getSensorId() == sensorId) {
                task.enabled = false;
                Serial.printf("Disabled sensor %s\n", sensorId.c_str());
                break;
            }
        }
    }
    
    /**
     * @brief Process all sensors that are ready for updates
     * 
     * Checks each enabled sensor against their intervals using persistent timing.
     * Handles both scheduled updates and immediate sensor needs (interrupts).
     * Updates RTC timing variables for next sleep cycle.
     */
    void checkAndUpdateAll() {
        for (auto& task : tasks) {
            if (!task.enabled) continue;
            
            bool intervalDue = (*task.lastUpdate == 0) || (currentWakeTime - *task.lastUpdate) >= task.interval;
            bool immediateNeed = task.sensor->needsUpdate();
            
            if (intervalDue || immediateNeed) {
                Serial.printf("Updating sensor %s (interval: %s, immediate: %s, firstBoot: %s)\n", 
                             task.sensor->getSensorId().c_str(),
                             (intervalDue && !firstBoot) ? "YES" : "NO",
                             immediateNeed ? "YES" : "NO",
                             firstBoot ? "YES" : "NO");
                
                task.sensor->handle();
                *task.lastUpdate = currentWakeTime;
            }
        }
        
        // Update persistent timing for next wake cycle
        schedulerLastWakeTime = currentWakeTime;
    }
    
    /**
     * @brief Calculate the next wake time for deep sleep optimization
     * @return Milliseconds until next sensor update is due
     * 
     * Finds the earliest required wake time across all enabled sensors.
     * Uses persistent timing to work across deep sleep cycles.
     * Returns shortest interval for sleep timer configuration.
     */
    unsigned long getNextWakeTime() {
        unsigned long shortestInterval = ULONG_MAX;
        
        for (const auto& task : tasks) {
            if (!task.enabled) continue;
            
            if (task.sensor->needsUpdate()) {
                return 0; // Immediate wake needed
            }
            
            unsigned long timeSinceLastUpdate = currentWakeTime - *task.lastUpdate;
            unsigned long timeUntilNextUpdate = 0;
            
            if (timeSinceLastUpdate < task.interval) {
                timeUntilNextUpdate = task.interval - timeSinceLastUpdate;
            }
            
            if (timeUntilNextUpdate < shortestInterval) {
                shortestInterval = timeUntilNextUpdate;
            }
        }
        
        return (shortestInterval == ULONG_MAX) ? 60000 : shortestInterval; // Default 60s
    }
    
    /**
     * @brief Prepare for deep sleep by storing timing information
     * @param sleepTimeMs Milliseconds the system will sleep
     * 
     * Updates RTC persistent variables so scheduler can track time
     * across deep sleep cycles. Call before esp_deep_sleep_start().
     */
    void prepareSleep(unsigned long sleepTimeMs) {
        schedulerSleepDuration = sleepTimeMs;
        //Serial.printf("Preparing sleep for %lu ms\n", sleepTimeMs);
    }
    
    /**
     * @brief Check if any sensor has data ready for MQTT transmission
     * @return true if any sensor needs to send data immediately
     * 
     * Checks for immediate sensor needs OR if any sensor is due for update.
     * On first boot (lastUpdate = 0), all sensors are considered due.
     */
    bool hasDataToSend() {
        Serial.printf("Checking hasDataToSend... firstBoot: %s\n", firstBoot ? "YES" : "NO");
        
        for (const auto& task : tasks) {
            if (!task.enabled) continue;
            
            // Immediate needs (interrupts, etc.)
            if (task.sensor->needsUpdate()) {
                Serial.printf("Sensor %s needs immediate update\n", task.sensor->getSensorId().c_str());
                return true;
            }
            
            // First boot: all sensors should run
            if (firstBoot) {
                Serial.printf("First boot - sensor %s should run\n", task.sensor->getSensorId().c_str());
                return true;
            }
            
            // Scheduled updates (first time or interval elapsed)  
            bool isDue = false;
            unsigned long timeSinceUpdate = 0;
            
            if (*task.lastUpdate == 0) {
                isDue = true; // First time running this sensor
                timeSinceUpdate = 0;
            } else if (currentWakeTime >= *task.lastUpdate) {
                timeSinceUpdate = currentWakeTime - *task.lastUpdate;
                isDue = (timeSinceUpdate >= task.interval);
            } else {
                // Clock rollover or timing issue - force update to resync
                Serial.printf("Timing resync needed for %s (current: %lu < last: %lu)\n", 
                             task.sensor->getSensorId().c_str(), currentWakeTime, *task.lastUpdate);
                isDue = true;
                timeSinceUpdate = 0;
            }
            
            Serial.printf("Sensor %s: currentTime=%lu, lastUpdate=%lu, timeSince=%lu, interval=%lu, isDue=%s\n", 
                         task.sensor->getSensorId().c_str(), currentWakeTime, *task.lastUpdate, timeSinceUpdate, task.interval, isDue ? "YES" : "NO");
            
            if (isDue) {
                return true;
            }
        }
        Serial.println("No sensors need updating");
        return false;
    }
    
    /**
     * @brief Get count of active sensors
     * @return Number of enabled sensors in scheduler
     */
    size_t getActiveSensorCount() {
        size_t count = 0;
        for (const auto& task : tasks) {
            if (task.enabled) count++;
        }
        return count;
    }
    
    /**
     * @brief Print scheduler status for debugging
     * 
     * Outputs all sensor states, timing, and intervals for troubleshooting.
     * Shows persistent timing across deep sleep cycles.
     */
    void printStatus() {
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        String wakeReason = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) ? "Rain Interrupt" : 
                           (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "Timer" : "Other";
        
        Serial.println("=== Sensor Scheduler Status ===");
        Serial.printf("Wake Reason: %s\n", wakeReason.c_str());
        Serial.printf("Current Wake Time: %lu ms\n", currentWakeTime);
        Serial.printf("Last Wake Time: %lu ms\n", schedulerLastWakeTime);
        
        // Calculate actual elapsed time safely to avoid underflow
        unsigned long actualElapsed = 0;
        if (currentWakeTime >= schedulerLastWakeTime) {
            actualElapsed = currentWakeTime - schedulerLastWakeTime;
        } else {
            // Handle millis() overflow (occurs every ~49.7 days)
            actualElapsed = (ULONG_MAX - schedulerLastWakeTime) + currentWakeTime + 1;
        }
        Serial.printf("Actual Elapsed: %lu ms\n", actualElapsed);
        
        for (const auto& task : tasks) {
            unsigned long timeSinceUpdate = currentWakeTime - *task.lastUpdate;
            bool isDue = timeSinceUpdate >= task.interval;
            
            Serial.printf("Sensor: %s | Enabled: %s | Due: %s | Last: %lums ago | Interval: %lums\n",
                         task.sensor->getSensorId().c_str(),
                         task.enabled ? "YES" : "NO",
                         isDue ? "YES" : "NO",
                         timeSinceUpdate,
                         task.interval);
        }
        Serial.println("==============================");
    }
};

#endif