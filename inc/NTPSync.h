#ifndef NTPSYNC_H
#define NTPSYNC_H

#include "Arduino.h"
#include <WiFi.h>
#include <time.h>
#include "esp_sntp.h"

// RTC persistent variables for NTP sync status
RTC_DATA_ATTR bool ntpSynced = false;
RTC_DATA_ATTR unsigned long lastNtpSyncTime = 0; // Using same timebase as SensorScheduler

/**
 * @brief NTP time synchronization manager for ESP32 IoT devices
 * 
 * Provides accurate time synchronization for timestamping sensor data and logs.
 * Optimized for deep sleep applications with persistent sync status tracking.
 * 
 * Features:
 * - Automatic timezone configuration
 * - RTC persistent sync status across deep sleep cycles
 * - Configurable sync intervals to minimize power consumption
 * - Multiple NTP server support for reliability
 * - Battery-friendly sync scheduling
 */
class NTPSync {
private:
    const char* timezone;
    unsigned long syncIntervalMs;
    bool initialized;
    
public:
    /**
     * @brief Constructs NTP synchronization manager
     * @param tz Timezone string (e.g., "EST5EDT,M3.2.0,M11.1.0" for US Eastern)
     * @param syncInterval Sync interval in milliseconds (default: 24 hours)
     * 
     * Common timezone examples:
     * - "UTC0" - UTC time
     * - "EST5EDT,M3.2.0,M11.1.0" - US Eastern (auto DST)
     * - "PST8PDT,M3.2.0,M11.1.0" - US Pacific (auto DST)
     * - "CET-1CEST,M3.5.0,M10.5.0/3" - Central European (auto DST)
     */
    NTPSync(const char* tz = "UTC0", unsigned long syncInterval = 86400000) 
        : timezone(tz), syncIntervalMs(syncInterval), initialized(false) {
    }
    
    /**
     * @brief Initialize NTP client with multiple servers
     * @return true if initialization successful
     * 
     * Configures SNTP with reliable NTP servers and timezone.
     * Call after WiFi connection is established.
     * Uses pool.ntp.org servers for global coverage.
     */
    bool begin() {
        if (!WiFi.isConnected()) {
            Serial.println("NTP: WiFi not connected");
            return false;
        }
        
        // Configure timezone
        setenv("TZ", timezone, 1);
        tzset();
        
        // Configure SNTP with multiple servers for reliability
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_setservername(2, "time.google.com");
        esp_sntp_init();
        
        initialized = true;
        Serial.printf("(%dms) NTP: Initialized with timezone %s\n", millis(), timezone);
        return true;
    }
    
    /**
     * @brief Perform time synchronization with timeout
     * @param currentTime Current time from SensorScheduler timebase for persistent tracking
     * @param timeoutMs Maximum wait time in milliseconds (default: 10 seconds)
     * @return true if sync successful, false on timeout or error
     * 
     * Waits for NTP response and updates system time.
     * Battery-optimized with configurable timeout.
     * Updates persistent sync status using SensorScheduler timebase for deep sleep tracking.
     */
    bool sync(unsigned long currentTime, unsigned long timeoutMs = 10000) {
        if (!initialized) {
            Serial.println("NTP: Not initialized");
            return false;
        }
        
        Serial.printf("(%dms) NTP: Synchronizing...\n", millis());
        unsigned long startTime = millis();
        
        // Wait for time synchronization
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
            if (millis() - startTime > timeoutMs) {
                Serial.println("NTP: Sync timeout");
                return false;
            }
            delay(100);
        }
        
        // Update persistent sync status using SensorScheduler timebase
        ntpSynced = true;
        lastNtpSyncTime = currentTime;
        
        // Print synchronized time
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        Serial.printf("NTP: Synced to %04d-%02d-%02d %02d:%02d:%02d\n",
                     timeinfo->tm_year + 1900,
                     timeinfo->tm_mon + 1,
                     timeinfo->tm_mday,
                     timeinfo->tm_hour,
                     timeinfo->tm_min,
                     timeinfo->tm_sec);
        
        return true;
    }
    
    /**
     * @brief Check if time sync is needed based on interval
     * @param currentTime Current time from SensorScheduler timebase
     * @return true if sync is due or never performed
     * 
     * Uses RTC persistent variables to track sync status across deep sleep.
     * Accounts for battery conservation by avoiding frequent syncs.
     * Uses SensorScheduler timebase for accurate interval tracking across sleep cycles.
     */
    bool needsSync(unsigned long currentTime) {
        if (!ntpSynced) {
            Serial.println("NTP: Never synced, sync needed");
            return true; // Never synced
        }
        
        // Check if sync interval has elapsed using persistent timebase
        if (currentTime >= lastNtpSyncTime) {
            unsigned long timeSinceSync = currentTime - lastNtpSyncTime;
            bool syncDue = (timeSinceSync >= syncIntervalMs);
            
            if (syncDue) {
                Serial.printf("NTP: Sync due - %lu ms since last sync (interval: %lu ms)\n", 
                             timeSinceSync, syncIntervalMs);
            } else {
                Serial.printf("NTP: Sync not needed - %lu ms since last sync (interval: %lu ms)\n", 
                             timeSinceSync, syncIntervalMs);
            }
            
            return syncDue;
        } else {
            // Clock rollover or timing issue - force resync
            Serial.printf("NTP: Timing resync needed (current: %lu < last: %lu)\n", 
                         currentTime, lastNtpSyncTime);
            return true;
        }
    }
    
    /**
     * @brief Get current Unix timestamp
     * @return Unix timestamp in seconds, or 0 if time not set
     * 
     * Returns accurate timestamp for sensor data logging.
     * Works across deep sleep cycles once initially synced.
     */
    time_t getUnixTime() {
        return time(nullptr);
    }
    
    /**
     * @brief Get current time as formatted string
     * @param buffer Character buffer for output (min 20 bytes)
     * @param format strftime format string (default: ISO 8601)
     * @return true if successful, false if time not available
     * 
     * Example formats:
     * - "%Y-%m-%dT%H:%M:%SZ" - ISO 8601 UTC
     * - "%Y-%m-%d %H:%M:%S" - Simple datetime
     * - "%H:%M:%S" - Time only
     */
    bool getTimeString(char* buffer, const char* format = "%Y-%m-%dT%H:%M:%SZ") {
        time_t now = getUnixTime();
        if (now < 1000000000) { // Sanity check for valid timestamp
            return false;
        }
        
        struct tm* timeinfo = localtime(&now);
        strftime(buffer, 32, format, timeinfo);
        return true;
    }
    
    /**
     * @brief Check if system time is valid
     * @return true if time appears to be correctly set
     * 
     * Performs sanity check on system time.
     * Useful for validating time before timestamping data.
     */
    bool isTimeValid() {
        time_t now = getUnixTime();
        return (now > 1000000000); // After 2001 (basic sanity check)
    }
    
    /**
     * @brief Get sync status information
     * @return true if NTP has been synced since boot/power-on
     */
    bool isSynced() {
        return ntpSynced;
    }
    
    /**
     * @brief Stop NTP service to save power
     * 
     * Call before deep sleep to minimize power consumption.
     * Sync status persists across sleep cycles.
     */
    void stop() {
        if (initialized) {
            esp_sntp_stop();
            Serial.println("NTP: Service stopped");
        }
    }
};

#endif