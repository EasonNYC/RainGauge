
#include <OneWire.h>

 // on pin 10 (a 4.7K resistor is necessary)

class Tempsensor{
  
public:
  Tempsensor(uint8_t pin, PubSubClient* cli, MqttMessageQueue* q, String top)
  :ds(pin),type_s(0),client(cli),tx_queue(q),topic(top)
  {
    saved_pin = pin;
  }

  ~Tempsensor(){
   
  }

  void begin(){

    Serial.printf("Started Soiltemp on pin %d\n", saved_pin);
    
    if ( !ds.search(addr)) {
        Serial.println("No more addresses.");
        ds.reset_search();
        delay(250);   
    }

    startConversion();

  }
  
  void startConversion(){

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
    convMillis = millis();
    sensorState = 1;

  }
  void waitForDataReady(){
    if((millis() - convMillis) > 1000) {
      sensorState = 2;
    }

  }

  void readData(){
    ds.reset();             //present
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad
 
    for (int i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
      //  WebSerial.print(data[i], HEX);
      //  WebSerial.print(" ");
    }
    sensorState = 0;
  }

  float getC(){
    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    float celsius = (float)raw / 16.0;
    return celsius;
  }

  float getF(){
    float celcius = getC();
    return celcius * 1.8 + 32.0;
  }

  void handle(){

    readData();
    reportF();

    JsonDocument myObject;

    myObject["soil_temp"] = getF();

    tx_queue->enqueue(topic.c_str(), myObject);

  }
    
  void reportF(){
    Serial.printf("(%dms) Soil Temp = %fF\n", millis(), getF());
  }

private:
    //const uint8_t PIN;
    OneWire ds;
    int saved_pin;
    byte data[9];
    byte addr[8];
    byte type_s;
    PubSubClient* client;
    MqttMessageQueue* tx_queue;
    String topic;
    int sensorState = 0;
    volatile unsigned long convMillis = 0;
    
};