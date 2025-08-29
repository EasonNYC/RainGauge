#ifndef DEBUGMANAGER_H
#define DEBUGMANAGER_H

#include "Arduino.h"
#include <WiFi.h>
#include "esp_sleep.h"
#include "OTA.h"

/**
 * @brief Debug and power management controller for IoT device development
 * 
 * This class manages the operational mode of ESP32-based IoT devices, switching
 * between normal operation (with deep sleep power savings) and debug mode (with
 * OTA firmware updates). Features:
 * 
 * - Hardware pin-based debug mode selection
 * - Automatic deep sleep management for battery conservation
 * - Integrated OTA update functionality during debug sessions
 * - Dynamic mode switching based on pin state
 * - WiFi management tied to operational mode
 * 
 * Operational Modes:
 * - Normal Mode: Executes sensor readings and enters deep sleep for power savings
 * - Debug Mode: Stays awake, enables WiFi, and allows OTA firmware updates
 * 
 * Essential for deployed IoT sensors that need both power efficiency in production
 * and convenient remote debugging/updating capabilities during development.
 */
class DebugManager {
private:
    int debug_pin;
    bool debug_mode;
    OTAManager* ota;
    
public:
    /**
     * @brief Constructs a DebugManager with hardware pin and OTA integration
     * @param debug_pin GPIO pin number used for debug mode selection
     * @param ota_manager Pointer to OTAManager instance for firmware updates
     * 
     * Configures debug pin as INPUT_PULLUP. Pin logic:
     * - HIGH (floating/VCC): Debug mode enabled
     * - LOW (GND): Normal operation mode
     * 
     * OTA manager used exclusively during debug mode for firmware updates.
     */
    DebugManager(int debug_pin, OTAManager* ota_manager) 
        : debug_pin(debug_pin), debug_mode(false), ota(ota_manager) {
        pinMode(debug_pin, INPUT_PULLUP);
    }
    
    /**
     * @brief Reads debug pin state and updates operational mode
     * @return true if debug mode is enabled, false for normal operation
     * 
     * Pin logic: HIGH = debug mode (stays awake, WiFi, OTA), LOW = normal
     * (sensor readings, deep sleep). Updates internal state and prints mode.
     * 
     * Call during startup and periodically to detect hardware mode changes.
     * Serial output confirms current mode for troubleshooting.
     */
    bool checkDebugModePin() {
        if (digitalRead(debug_pin)) {
            Serial.println("DEBUG MODE: ON");
            debug_mode = true;
        } else {
            Serial.println("DEBUG MODE: OFF");
            debug_mode = false;
        }

        return debug_mode;
    }
    
    /**
     * @brief Returns the current debug mode state
     * @return true if in debug mode, false if in normal operation mode
     * 
     * Read-only access to current mode without hardware pin check.
     * Used for conditional power-saving, logging, WiFi initialization,
     * and sleep/wake decisions.
     * 
     * Returns internal flag set by checkDebugModePin().
     */
    bool getDebugMode() const {
        return debug_mode;
    }
    
    /**
     * @brief Initializes debug mode with WiFi connection and OTA services
     * @param connectWifi Function pointer to WiFi connection routine
     * @param ota_port Network port for OTA communication (typically 3232)
     * @param ota_hostname Device hostname visible during OTA updates
     * @param ota_password Password for OTA authentication
     * 
     * Only activates if debug_mode enabled. Calls WiFi function, starts OTA
     * manager, makes device discoverable for firmware updates.
     * 
     * No action in normal mode, preserving battery efficiency.
     * Call once during setup after checking debug mode.
     */
    void startDebugMode(void (*connectWifi)(), int ota_port, const char* ota_hostname, const char* ota_password) {
        if (debug_mode) {
            // Connect to wifi early for OTA
            connectWifi();
            
            // Start OTA
            ota->begin(ota_port, ota_hostname, ota_password);
        }
    }

    /**
     * @brief Initiates ESP32 deep sleep for power conservation
     * 
     * Only sleeps when NOT in debug mode. Reduces current from ~240mA to <1mA.
     * Uses fixed TIME_TO_SLEEP duration.
     * 
     * Process: Prints notification, flushes serial, disconnects WiFi,
     * enters deep sleep until timer/external wakeup.
     * 
     * Debug mode protection: No sleep occurs, preserving WiFi and OTA.
     * WARNING: Method does not return - device resets on wakeup.
     */
    void enterSleepMode() {
        if (!debug_mode) {
            // DEEP SLEEP
            Serial.printf("(%dms) Sleeping now...\n", millis());
            Serial.flush();
            
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            
            esp_deep_sleep_start();
        }
    }
    
    /**
     * @brief Initiates ESP32 deep sleep with custom duration
     * @param sleepTimeMs Milliseconds to sleep
     * 
     * Dynamic sleep timing version for sensor scheduler optimization.
     * Reconfigures timer wakeup with provided duration before sleeping.
     */
    void enterSleepMode(unsigned long sleepTimeMs) {
        if (!debug_mode) {
            // Reconfigure sleep timer for dynamic duration
            esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000ULL);
            
            Serial.printf("(%dms) Sleeping for %lu ms...\n", millis(), sleepTimeMs);
            Serial.flush();
            
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            
            esp_deep_sleep_start();
        }
    }
    
    /**
     * @brief Main processing method for debug/power management in application loop
     * 
     * Core operational logic for continuous main loop calls.
     * 
     * Debug mode: Processes OTA requests, monitors pin for mode exit
     * (HIGH to LOW), allows real-time mode switching.
     * 
     * Normal mode: Immediately enters deep sleep for battery conservation.
     * 
     * Centralizes power management decisions for both operational modes.
     */
    void handle() {
        if (debug_mode) {
            // Handle OTA updates
            ota->handle();
            
            // Check if user wants to exit debug mode
            if (digitalRead(debug_pin) == 0) {
                debug_mode = false;
                Serial.println("Exiting OTA mode...");
            }
        } else {
            enterSleepMode();
        }
    }
    
    /**
     * @brief Main processing method with dynamic sleep timing
     * @param sleepTimeMs Milliseconds to sleep (from sensor scheduler)
     * 
     * Same as handle() but allows custom sleep duration instead of fixed TIME_TO_SLEEP.
     * Use this version with SensorScheduler for optimized sleep timing.
     */
    void handle(unsigned long sleepTimeMs) {
        if (debug_mode) {
            // Handle OTA updates
            ota->handle();
            
            // Check if user wants to exit debug mode
            if (digitalRead(debug_pin) == 0) {
                debug_mode = false;
                Serial.println("Exiting OTA mode...");
            }
        } else {
            enterSleepMode(sleepTimeMs);
        }
    }
    
    
};

#endif