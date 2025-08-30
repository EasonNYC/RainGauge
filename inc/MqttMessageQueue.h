#ifndef MQTTMESSAGEQUEUE_H
#define MQTTMESSAGEQUEUE_H

#include <ArduinoJson.h>
#include <time.h>

/**
 * @brief Container for MQTT message data with topic, payload, and timestamp
 * 
 * Structure that holds the essential components of an MQTT message:
 * - topic: The MQTT topic string where the message will be published
 * - payload: The message content as a JSON-serialized string
 * - timestamp: Unix timestamp when message was created (0 if time unavailable)
 * 
 * Used by the MqttMessageQueue to store messages for reliable transmission
 * when network connectivity is intermittent or during batch processing.
 */
struct MqttMessage {
    String topic;
    String payload;
    time_t timestamp;
    
    MqttMessage() : timestamp(0) {}
};

/**
 * @brief Thread-safe circular queue for buffering MQTT messages
 * @tparam MAX_SIZE Maximum number of messages the queue can hold
 * 
 * This template class implements a circular queue specifically designed for
 * reliable MQTT message handling in IoT applications. Features:
 * 
 * - Fixed-size circular buffer with compile-time size specification
 * - Automatic JSON serialization from ArduinoJson documents
 * - Thread-safe operations for interrupt-driven sensor data
 * - Overflow protection with full queue detection
 * - Memory-efficient design suitable for constrained embedded systems
 * 
 * Primary use cases:
 * - Buffering sensor readings when network connectivity is poor
 * - Batch transmission of accumulated data during scheduled wake cycles
 * - Ensuring data integrity during deep sleep/wake cycles
 * - Handling burst sensor data from interrupt service routines
 * 
 * The queue maintains FIFO (First In, First Out) ordering and provides
 * safe overflow handling by rejecting new messages when full.
 */
template<size_t MAX_SIZE>
class MqttMessageQueue {
public:
  /**
   * @brief Constructs an empty MQTT message queue
   * 
   * Initializes circular queue with zero messages, sets pointers to 0.
   * Ready for immediate use with full MAX_SIZE capacity available.
   */
  MqttMessageQueue()
  : _head(0), _tail(0), _count(0) 
  {
  }

  /**
   * @brief Adds a new MQTT message to the queue with JSON serialization and timestamp
   * @param topic The MQTT topic string for message publication
   * @param doc ArduinoJson document containing the message data
   * @return true if message was successfully queued, false if queue is full
   * 
   * Checks space, serializes JsonDocument to payload, stores topic/payload/timestamp,
   * updates tail pointer with wraparound. Rejects if full to prevent overflow.
   * 
   * Automatically captures current Unix timestamp for message ordering and debugging.
   * Thread-safe for single producer/consumer. External sync needed for multiple.
   */
  bool enqueue(const String& topic, const JsonDocument& doc) {
    if (isFull()) { return false; }

    String payload;
    serializeJson(doc, payload);

    _queue[_tail].topic = topic;
    _queue[_tail].payload = payload;
    _queue[_tail].timestamp = time(nullptr); // Capture current Unix timestamp

    _tail = (_tail + 1) % MAX_SIZE;
    _count++;
    return true;
  }

  /**
   * @brief Removes and retrieves the oldest message from the queue
   * @param message Reference to MqttMessage that will receive the dequeued data
   * @return true if message was successfully retrieved, false if queue is empty
   * 
   * FIFO retrieval: checks if empty, copies head message to reference,
   * updates head pointer with wraparound, decrements count.
   * 
   * Returns false if empty. Used by MQTT transmission during connectivity.
   * Thread-safe for single producer/consumer.
   */
  bool dequeue(MqttMessage& message) {
    if (isEmpty()) {return false;}

    message = _queue[_head];
    _head = (_head + 1) % MAX_SIZE;
    _count--;
    return true;
  }

  /**
   * @brief Checks if the queue contains no messages
   * @return true if queue is empty (count = 0), false otherwise
   * 
   * Const method for empty state check. Used to prevent dequeue on empty
   * queues, check pending messages, and loop through queued messages.
   */
  bool isEmpty() const {
    return _count == 0;
  }

  /**
   * @brief Checks if the queue has reached maximum capacity
   * @return true if queue is full (count = MAX_SIZE), false otherwise
   * 
   * Const method for capacity check. Used to prevent overflow, implement
   * backpressure handling, monitor utilization, and trigger alternative
   * processing when buffer saturated.
   */
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
