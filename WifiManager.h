#pragma once
#include <WiFi.h>
#include "Secrets.h"

class WiFiManager {
public:
  const char* ssid;
  const char* password;
  uint8_t* bssid = nullptr;
  int channel = 0;
  bool use_static_ip = false;
  IPAddress local_ip, gateway, subnet, dns;

  WiFiManager(const char* ssid, const char* password) 
  : ssid(ssid), password(password) 
  {}

  void setStaticIP(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns_server) {
    use_static_ip = true;
    local_ip = ip;
    gateway = gw;
    subnet = sn;
    dns = dns_server;
  }

  void setStaticIP(const char* ip_str, const char* gw_str, const char* sn_str, const char* dns_str) {
      if (!local_ip.fromString(ip_str) || !gateway.fromString(gw_str) || !subnet.fromString(sn_str) || !dns.fromString(dns_str)) 
      {
         Serial.println("❌ Invalid IP address format in setStaticIP()");
         return;
      }
      use_static_ip = true;
  }

  void setFastConnect(uint8_t* ap_bssid, int ap_channel) {
    bssid = ap_bssid;
    channel = ap_channel;
  }

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