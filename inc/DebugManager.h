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
     * Initializes the debug management system with hardware control:
     * - Configures debug pin as INPUT_PULLUP (HIGH = debug mode, LOW = normal)
     * - Sets initial mode to normal operation (debug_mode = false)
     * - Stores reference to OTA manager for debug mode functionality
     * 
     * Pin Configuration:
     * - Pull-up enabled: Pin reads HIGH when floating/disconnected
     * - Connect to GND to enable debug mode (LOW state)
     * - Connect to VCC or leave floating for normal mode (HIGH state)
     * 
     * The OTA manager is used exclusively during debug mode for remote
     * firmware updates and must remain valid for the object's lifetime.
     */
    DebugManager(int debug_pin, OTAManager* ota_manager) 
        : debug_pin(debug_pin), debug_mode(false), ota(ota_manager) {
        pinMode(debug_pin, INPUT_PULLUP);
    }
    
    /**
     * @brief Reads debug pin state and updates operational mode
     * @return true if debug mode is enabled, false for normal operation
     * 
     * Samples the hardware debug pin to determine the current operational mode
     * and updates internal state accordingly. Pin Logic:
     * - HIGH (3.3V or floating): Enables debug mode, prints "DEBUG MODE: ON"  
     * - LOW (GND): Normal operation mode, prints "DEBUG MODE: OFF"
     * 
     * Mode Effects:
     * - Debug Mode (pin HIGH): Device stays awake, enables WiFi and OTA updates
     * - Normal Mode (pin LOW): Device performs sensor readings and enters deep sleep
     * 
     * This method should be called during startup to establish initial mode
     * and can be called periodically to detect mode changes via hardware switch.
     * 
     * Serial output provides immediate feedback for mode confirmation during
     * development and troubleshooting.
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
     * Provides read-only access to the current operational mode without
     * modifying state or checking hardware pins. This const method allows
     * other components to query the debug state for conditional behavior.
     * 
     * Use cases:
     * - Conditional execution of power-saving features
     * - Enabling/disabling verbose logging based on mode
     * - Determining whether to initialize WiFi or OTA services
     * - Making decisions about sleep vs. active operation
     * 
     * Returns the internal debug_mode flag last set by checkDebugModePin()
     * or modified through other debug management methods.
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
     * Conditionally starts debug mode services only if debug_mode is enabled.
     * This prevents unnecessary WiFi activation and OTA initialization during
     * normal battery-powered operation.
     * 
     * Initialization sequence (debug mode only):
     * 1. Calls provided WiFi connection function to establish network
     * 2. Configures and starts OTA manager with specified parameters
     * 3. Device becomes discoverable for remote firmware updates
     * 
     * The connectWifi function should handle all WiFi setup including:
     * - SSID/password configuration
     * - Connection establishment and verification  
     * - Any required network-specific setup (static IP, etc.)
     * 
     * Call this method once during setup after determining debug mode status.
     * No action is taken if not in debug mode, preserving power efficiency.
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
     * Conditionally enters deep sleep mode only when NOT in debug mode,
     * providing automatic power management for battery-powered IoT devices.
     * Deep sleep dramatically reduces power consumption for long-term deployments.
     * 
     * Pre-sleep sequence (normal mode only):
     * 1. Prints sleep notification with timestamp for debugging
     * 2. Flushes serial buffer to ensure message transmission
     * 3. Disconnects WiFi and disables WiFi radio (power saving)
     * 4. Enters deep sleep until next wakeup event
     * 
     * Power Savings:
     * - Deep sleep reduces current from ~240mA to <1mA
     * - Only RTC and wakeup sources remain powered
     * - Device wakes on timer or external interrupt (rain gauge)
     * 
     * Debug Mode Protection:
     * - No sleep occurs in debug mode, allowing continuous operation
     * - Enables uninterrupted OTA updates and serial monitoring
     * - Preserves WiFi connection for development activities
     * 
     * WARNING: This method does not return - device resets on wakeup.
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
     * @brief Main processing method for debug/power management in application loop
     * 
     * This method implements the core operational logic that should be called
     * continuously from the main application loop. It provides automatic
     * mode-specific behavior without requiring external conditional logic.
     * 
     * Debug Mode Operation:
     * - Processes OTA update requests via ota->handle()
     * - Monitors debug pin for mode exit (HIGH to LOW transition)
     * - Allows real-time switching from debug to normal mode
     * - Maintains WiFi connection and OTA availability
     * 
     * Normal Mode Operation:
     * - Immediately calls enterSleepMode() to conserve battery
     * - Device enters deep sleep until next wakeup event
     * - No further processing until device restart
     * 
     * Dynamic Mode Switching:
     * - Debug pin state is checked each loop iteration in debug mode
     * - Setting pin LOW during debug session exits to normal mode
     * - Provides hardware-controlled transition without code restart
     * 
     * This method centralizes the power management decision logic and
     * ensures appropriate behavior for both operational modes.
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
    
    
};

#endif