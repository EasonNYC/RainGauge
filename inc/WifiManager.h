#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>

/**
 * @brief WiFi connection manager with fast reconnect and static IP support
 * 
 * This class manages WiFi connections for ESP32 with optimizations for low-power
 * applications. It supports:
 * - Fast reconnection using stored credentials to minimize connection time
 * - Static IP configuration to avoid DHCP delays
 * - BSSID/channel caching for fastest possible reconnection
 * - Connection timeout handling with detailed logging
 */
class WiFiManager {
public:
  const char* ssid;
  const char* password;
  uint8_t* bssid = nullptr;
  int channel = 0;
  bool use_static_ip = false;
  IPAddress local_ip, gateway, subnet, dns;

  /**
   * @brief Constructs a WiFiManager with network credentials
   * @param ssid The WiFi network name (SSID) to connect to
   * @param password The WiFi network password
   * 
   * Initializes with basic credentials. Use setter methods for additional
   * configuration like static IP or fast connect parameters.
   */
  WiFiManager(const char* ssid, const char* password) 
  : ssid(ssid), password(password) 
  {}

  /**
   * @brief Configures static IP settings using IPAddress objects
   * @param ip The static IP address for this device
   * @param gw The gateway IP address
   * @param sn The subnet mask
   * @param dns_server The DNS server IP address
   * 
   * Enables static IP, avoids DHCP delays, reduces connection time.
   */
  void setStaticIP(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns_server) {
    use_static_ip = true;
    local_ip = ip;
    gateway = gw;
    subnet = sn;
    dns = dns_server;
  }

  /**
   * @brief Configures static IP settings using string representations
   * @param ip_str The static IP address as a string (e.g., "192.168.1.100")
   * @param gw_str The gateway IP address as a string
   * @param sn_str The subnet mask as a string (e.g., "255.255.255.0")
   * @param dns_str The DNS server IP address as a string
   * 
   * String version with validation. Prints error if malformed, enables
   * static IP if valid to reduce connection time.
   */
  void setStaticIP(const char* ip_str, const char* gw_str, const char* sn_str, const char* dns_str) {
      if (!local_ip.fromString(ip_str) || !gateway.fromString(gw_str) || !subnet.fromString(sn_str) || !dns.fromString(dns_str)) 
      {
         Serial.println("❌ Invalid IP address format in setStaticIP()");
         return;
      }
      use_static_ip = true;
  }

  /**
   * @brief Configures fast connection parameters for minimal connection time
   * @param ap_bssid Pointer to the 6-byte BSSID (MAC address) of the access point
   * @param ap_channel The WiFi channel number (1-13) of the access point
   * 
   * Provides specific BSSID and channel to eliminate scanning, significantly
   * reducing connection time for battery-powered devices.
   */
  void setFastConnect(uint8_t* ap_bssid, int ap_channel) {
    bssid = ap_bssid;
    channel = ap_channel;
  }

  /**
   * @brief Connects to WiFi with boot-specific optimization strategy
   * @param bootNum Boot counter to determine connection strategy (2 = first-time setup)
   * 
   * bootNum == 2: First-time setup, saves credentials to flash
   * Other values: Fast reconnect using stored credentials
   * 
   * Process: Station mode, disable sleep, apply static IP, 3s timeout,
   * report time/IP on success. Optimized for battery applications.
   */
  void connect(int bootNum) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    if (use_static_ip) {
      WiFi.config(local_ip, gateway, subnet, dns);
    }

    if (bootNum == 2) {
      Serial.println("First-time setup: Saving WiFI credentials to flash...");
      WiFi.persistent(true);
      WiFi.begin(ssid, password, channel, bssid); // Save credentials permanently
    } else {
      WiFi.persistent(false); //don't write to flash all the time
      Serial.println("Fast reconnect: using stored credentials...");        
      WiFi.begin(); //fast (uses saved credentials. does not write to flash)
      //if (bssid && channel > 0) {        
      //  Serial.println("Fast reconnect: using provided credentials...");
      //  WiFi.begin(ssid, password, channel, bssid); //fastest (Provides all info to avoid scanning) avoids flash write of credentials)
      //} else {
      //  Serial.println("Fast reconnect: using stored credentials...");        
      //  WiFi.begin(); //fast (uses saved credentials. does not write to flash)
      //}
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 3000) {
      delay(10);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected in ");
      Serial.print(millis() - start);
      Serial.println(" ms");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi connection failed.");
    }
  }
};

#endif