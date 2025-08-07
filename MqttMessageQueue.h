#pragma once
#include <ArduinoJson.h>



struct MqttMessage {
    String topic;
    String payload;
};

class MqttMessageQueue {
public:
  MqttMessageQueue(size_t maxSize)
  : _maxSize(maxSize), _head(0), _tail(0), _count(0) 
  {
    _queue = new MqttMessage[_maxSize];
  }

  bool enqueue(const String& topic, const JsonDocument& doc) {
    if (isFull()) { return false; }

    String payload;
    serializeJson(doc, payload);

    _queue[_tail].topic = topic;
    _queue[_tail].payload = payload;

    _tail = (_tail + 1) % _maxSize;
    _count++;
    return true;
  }

  bool dequeue(MqttMessage& message) {
    if (isEmpty()) {return false;}

    message = _queue[_head];
    _head = (_head + 1) % _maxSize;
    _count--;
    return true;
  }

  bool isEmpty() const {
    return _count == 0;
  }

  bool isFull() const {
    return _count == _maxSize;
  }

private:
  size_t _maxSize;
  size_t _head;
  size_t _tail;
  size_t _count;
  MqttMessage* _queue;
};



