# ESP32 Rain Gauge Weather Station

A comprehensive weather monitoring system built on the ESP32 platform that measures rainfall, temperature, pressure, and battery levels with wireless data transmission via MQTT.

## Features

- **Rain Gauge Monitoring**: Precision rainfall measurement using tipping bucket sensor
- **Environmental Sensors**:
  - Soil temperature (DS18B20 OneWire sensor)
  - Atmospheric temperature and pressure (BMP280 I2C sensor)
  - Battery voltage monitoring
- **Low Power Design**: Deep sleep functionality with sensor-triggered wake-up
- **Wireless Connectivity**: WiFi with static IP configuration and MQTT data transmission
- **Over-The-Air Updates**: OTA programming support for remote firmware updates
- **Debug Mode**: Pin-controlled debug mode for development and testing

## Hardware Requirements

- ESP32 Development Board
- Tipping bucket rain gauge (connected to GPIO 27)
- DS18B20 temperature sensor with 4.7K pull-up resistor (GPIO 33)
- BMP280 temperature/pressure sensor (I2C)
- Battery monitoring circuit (connected to A1)
- Debug mode jumper (GPIO 12)

## Software Dependencies

- Arduino IDE with ESP32 support
- Libraries:
  - WiFi
  - PubSubClient (MQTT)
  - ArduinoJson
  - ArduinoOTA
  - OneWire
  - Adafruit_BMP280

## Configuration

1. Copy `Secrets-example.h` to `Secrets.h`
2. Update `Secrets.h` with your specific configuration:
   - WiFi credentials and network settings
   - MQTT broker details
   - OTA settings

## Operation Modes

### Normal Mode (Deep Sleep)
- Wakes up every 60 seconds (configurable) to collect and transmit sensor data
- Enters deep sleep between measurements for battery conservation
- Rain events trigger immediate wake-up via interrupt

### Debug Mode
- Activated by grounding GPIO 12 at startup
- Enables OTA programming
- Prevents deep sleep for continuous operation
- Exit debug mode by releasing GPIO 12

## Data Output

The system publishes JSON-formatted sensor data to MQTT topics:

```json
{
  "rain": 0.024,           // inches of rainfall in last hour
  "soil_temp": 72.5,       // soil temperature in Fahrenheit
  "bmp_temperature": 75.2, // air temperature in Fahrenheit
  "bmp_pressure": 101325,  // atmospheric pressure in Pascals
  "battery": 3.7           // battery voltage
}
```

## Power Management

- Utilizes ESP32 deep sleep between measurements
- Battery monitoring with voltage divider circuit
- Bluetooth disabled to reduce power consumption
- WiFi automatically disconnected before sleep

## Installation

1. Clone this repository
2. Install required Arduino libraries
3. Configure your secrets in `Secrets.h`
4. Upload to ESP32 board
5. Connect sensors according to pin assignments

## Pin Configuration

- GPIO 27: Rain gauge input (interrupt-enabled)
- GPIO 33: DS18B20 temperature sensor
- GPIO 12: Debug mode control (INPUT_PULLUP)
- A1: Battery voltage monitoring
- I2C: BMP280 sensor (default SDA/SCL pins)

## Calibration

- Rain gauge: 0.01193 inches per bucket tip (configurable in `Rain.h`)
- Battery voltage: Adjust voltage divider calculation in `vbat.h`

## Contributing

Feel free to submit issues and enhancement requests!

## License

This project is open source. Please check individual library licenses for their respective terms.