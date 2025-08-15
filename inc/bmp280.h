#include <Adafruit_BMP280.h>
#include "inc/MqttMessageQueue.h"

Adafruit_BMP280 bmp; // I2C

template<size_t QUEUE_SIZE>
class bmp280sensor{

  PubSubClient* client;
  MqttMessageQueue<QUEUE_SIZE>* tx_queue;
  String topic;

  public:
  bmp280sensor(PubSubClient* cli, MqttMessageQueue<QUEUE_SIZE>* q, String top)
  :client(cli),tx_queue(q),topic(top)
  {

  }
  ~bmp280sensor(){}

  void begin() {

     //if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
    if (!bmp.begin()) {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
        while (1) delay(10);
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    Serial.printf("Started bmp280 via I2C\n");
  }

  float getF(float celcius){
    return celcius * 1.8 + 32.0;
  }

  void handle(){

    float temperature;
    float pressure;

    //get data
    if (bmp.takeForcedMeasurement()) {

        // can now print out the new measurements
        temperature = getF(bmp.readTemperature());
        pressure = bmp.readPressure();

        //print to the screen
        Serial.printf("(%dms) Outdoor Temperature = %f *F\n", millis(), temperature);
        Serial.printf("(%dms) Outdoor Pressure = %f Pa\n",  millis(), pressure);

    } else {
        Serial.println("BMP Forced measurement failed!");
    }

    JsonDocument myObject;

    myObject["bmp_temperature"] = temperature;
    myObject["bmp_pressure"] = pressure;

    tx_queue->enqueue(topic.c_str(), myObject);
    }

};