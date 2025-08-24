#ifndef OTA_H
#define OTA_H

#include <ArduinoOTA.h>

/**
 * @brief Over-The-Air (OTA) update manager for ESP32 firmware updates
 * 
 * This class provides a simple interface for configuring and managing OTA firmware
 * updates using the Arduino OTA library. Features:
 * 
 * - Password-protected OTA updates for security
 * - Progress monitoring with serial output
 * - Comprehensive error reporting and handling
 * - Support for both sketch and filesystem updates
 * - Automatic update type detection (flash vs SPIFFS)
 * 
 * Enables remote firmware updates over WiFi without physical access to the device,
 * essential for deployed IoT sensors in remote locations. Includes detailed logging
 * for troubleshooting update failures and monitoring progress.
 */
class OTAManager {
public:
    /**
     * @brief Initializes and configures OTA update functionality
     * @param port Network port for OTA communication (typically 3232)
     * @param hostname Device hostname visible on network during OTA updates
     * @param password Password required for OTA update authentication
     * 
     * Sets up the complete OTA update system with security and monitoring:
     * 
     * Configuration:
     * - Sets network port, hostname, and password for secure access
     * - Configures callback handlers for all OTA events
     * 
     * Event Handlers:
     * - onStart: Detects update type (sketch/filesystem) and logs start
     * - onEnd: Logs completion of update process
     * - onProgress: Displays percentage progress during update
     * - onError: Provides detailed error reporting for failures
     * 
     * Error Types Handled:
     * - OTA_AUTH_ERROR: Authentication/password failure
     * - OTA_BEGIN_ERROR: Update initialization failure
     * - OTA_CONNECT_ERROR: Network connection issues
     * - OTA_RECEIVE_ERROR: Data reception problems
     * - OTA_END_ERROR: Update completion failure
     * 
     * Call this method once during setup after WiFi connection is established.
     * The device will be discoverable for OTA updates until reboot or power cycle.
     */
    void begin(int port, const char* hostname, const char* password) {
        ArduinoOTA.setPort(port);
        ArduinoOTA.setHostname(hostname);
        ArduinoOTA.setPassword(password);

        ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() {
            Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });
        
        ArduinoOTA.begin();
        Serial.println("OTA Mode Active");
    }

    /**
     * @brief Processes incoming OTA update requests and manages update workflow
     * 
     * This method must be called regularly from the main application loop to
     * handle OTA update requests. It processes incoming network packets and
     * manages the complete update workflow when an OTA session is initiated.
     * 
     * Responsibilities:
     * - Monitors for incoming OTA connection attempts
     * - Handles authentication and session management
     * - Processes firmware/filesystem data reception
     * - Manages update progress and completion
     * - Triggers appropriate callback functions
     * 
     * During an active OTA update, this method will block normal application
     * execution to prioritize the update process. The device will automatically
     * restart after successful update completion.
     * 
     * Call this method frequently (every loop iteration) when OTA updates
     * are enabled to ensure responsive handling of update requests.
     */
    void handle() {
        ArduinoOTA.handle();
    }
};

#endif