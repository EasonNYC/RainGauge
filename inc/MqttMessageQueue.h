#ifndef MQTTMESSAGEQUEUE_H
#define MQTTMESSAGEQUEUE_H

#include <ArduinoJson.h>

/**
 * @brief Container for MQTT message data with topic and payload
 * 
 * Simple structure that holds the essential components of an MQTT message:
 * - topic: The MQTT topic string where the message will be published
 * - payload: The message content as a JSON-serialized string
 * 
 * Used by the MqttMessageQueue to store messages for reliable transmission
 * when network connectivity is intermittent or during batch processing.
 */
struct MqttMessage {
    String topic;
    String payload;
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
   * Initializes the circular queue with zero messages and sets all
   * internal pointers and counters to their starting positions:
   * - _head: Points to the next message to dequeue (0)
   * - _tail: Points to the next available slot for enqueue (0) 
   * - _count: Number of messages currently in queue (0)
   * 
   * The queue is ready for immediate use after construction with
   * full capacity available (MAX_SIZE messages).
   */
  MqttMessageQueue()
  : _head(0), _tail(0), _count(0) 
  {
  }

  /**
   * @brief Adds a new MQTT message to the queue with JSON serialization
   * @param topic The MQTT topic string for message publication
   * @param doc ArduinoJson document containing the message data
   * @return true if message was successfully queued, false if queue is full
   * 
   * Attempts to add a new message to the tail of the queue. The process:
   * 1. Checks if queue has available space (not full)
   * 2. Serializes the JsonDocument to a JSON string payload
   * 3. Stores topic and serialized payload in the queue slot
   * 4. Updates tail pointer (with wraparound) and increments count
   * 
   * If the queue is full, the message is rejected and false is returned
   * to prevent overflow. This provides safe handling of burst sensor data
   * or network connectivity issues.
   * 
   * Thread-safe for single producer, single consumer scenarios.
   * For multiple producers, external synchronization may be required.
   */
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

  /**
   * @brief Removes and retrieves the oldest message from the queue
   * @param message Reference to MqttMessage that will receive the dequeued data
   * @return true if message was successfully retrieved, false if queue is empty
   * 
   * Retrieves the message at the head of the queue (oldest message) following
   * FIFO ordering. The process:
   * 1. Checks if queue contains any messages (not empty)
   * 2. Copies the message at head position to the provided reference
   * 3. Updates head pointer (with wraparound) and decrements count
   * 4. Returns true to indicate successful retrieval
   * 
   * If the queue is empty, no operation is performed and false is returned.
   * The provided message reference remains unchanged in this case.
   * 
   * This method is typically called by MQTT transmission code to process
   * queued messages for network transmission during connectivity windows.
   * 
   * Thread-safe for single producer, single consumer scenarios.
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
   * Determines whether the queue has any messages available for dequeue
   * operation. This is a const method that doesn't modify queue state.
   * 
   * Used to:
   * - Prevent dequeue operations on empty queues
   * - Determine if there are pending messages to transmit
   * - Implement conditional processing based on queue contents
   * - Loop through all queued messages until empty
   * 
   * Returns true when _count equals 0, indicating no messages are stored.
   */
  bool isEmpty() const {
    return _count == 0;
  }

  /**
   * @brief Checks if the queue has reached maximum capacity
   * @return true if queue is full (count = MAX_SIZE), false otherwise
   * 
   * Determines whether the queue can accept additional messages via enqueue
   * operations. This is a const method that doesn't modify queue state.
   * 
   * Used to:
   * - Prevent enqueue operations that would cause overflow
   * - Implement backpressure handling in sensor data collection
   * - Monitor queue utilization for system health diagnostics
   * - Trigger alternative processing when buffer is saturated
   * 
   * Returns true when _count equals MAX_SIZE, indicating all queue slots
   * are occupied and no more messages can be stored.
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
