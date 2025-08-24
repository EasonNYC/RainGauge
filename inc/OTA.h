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
     * Sets up OTA system with security, progress monitoring, and error handling.
     * Configures callbacks for start/end/progress/error events.
     * 
     * Handles auth, initialization, connection, reception, and completion errors.
     * Call once after WiFi connection. Device discoverable until reboot.
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
     * Must be called regularly from main loop. Monitors for connections,
     * handles authentication, processes data, manages progress.
     * 
     * Blocks normal execution during active updates, prioritizing update
     * process. Device restarts automatically after successful completion.
     * 
     * Call every loop iteration for responsive OTA request handling.
     */
    void handle() {
        ArduinoOTA.handle();
    }
};

#endif