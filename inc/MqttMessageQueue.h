#ifndef MQTTMESSAGEQUEUE_H
#define MQTTMESSAGEQUEUE_H

#include <ArduinoJson.h>

struct MqttMessage {
    String topic;
    String payload;
};

template<size_t MAX_SIZE>
class MqttMessageQueue {
public:
  MqttMessageQueue()
  : _head(0), _tail(0), _count(0) 
  {
  }

  bool enqueue(const String& topic, const JsonDocument& doc) {
    if (isFull()) { return false; }

    String payload;
    serializeJson(doc, payload);

    _queue[_tail].topic = topic;
    _queue[_tail].payload = payload;

    _tail = (_tail + 1) % MAX_SIZE;
    _count++;
    return true;
  }

  bool dequeue(MqttMessage& message) {
    if (isEmpty()) {return false;}

    message = _queue[_head];
    _head = (_head + 1) % MAX_SIZE;
    _count--;
    return true;
  }

  bool isEmpty() const {
    return _count == 0;
  }

  bool isFull() const {
    return _count == MAX_SIZE;
  }

private:
  size_t _head;
  size_t _tail;
  size_t _count;
  MqttMessage _queue[MAX_SIZE];
};

#endif
