#ifndef DEBUGMANAGER_H
#define DEBUGMANAGER_H

#include "Arduino.h"
#include <WiFi.h>
#include "esp_sleep.h"
#include "OTA.h"

class DebugManager {
private:
    int debug_pin;
    bool debug_mode;
    OTAManager* ota;
    
public:
    DebugManager(int debug_pin, OTAManager* ota_manager) 
        : debug_pin(debug_pin), debug_mode(false), ota(ota_manager) {
        pinMode(debug_pin, INPUT_PULLUP);
    }
    
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
    
    bool getDebugMode() const {
        return debug_mode;
    }
    
    void startDebugMode(void (*connectWifi)(), int ota_port, const char* ota_hostname, const char* ota_password) {
        if (debug_mode) {
            // Connect to wifi early for OTA
            connectWifi();
            
            // Start OTA
            ota->begin(ota_port, ota_hostname, ota_password);
        }
    }

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