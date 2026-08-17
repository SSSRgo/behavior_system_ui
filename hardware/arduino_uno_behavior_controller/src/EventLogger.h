#pragma once

#include <Arduino.h>

#include "config.h"
#include "events.h"

struct EventRecord {
  uint32_t timestampUs;
  uint16_t trialNumber;
  int32_t value;
  EventID eventId;
};

class EventLogger {
 public:
  void begin();
  bool log(EventID eventId, uint16_t trialNumber = 0, int32_t value = 0);
  bool logAt(uint32_t timestampUs, EventID eventId, uint16_t trialNumber = 0, int32_t value = 0);
  bool logFromISR(uint32_t timestampUs, EventID eventId, uint16_t trialNumber = 0, int32_t value = 0);
  void flush(uint8_t maxRecords = Config::MAX_LOG_RECORDS_PER_LOOP);

  uint16_t droppedCount() const { return droppedCount_; }
  uint8_t queuedCount() const;

 private:
  static_assert(
      (Config::EVENT_QUEUE_CAPACITY & (Config::EVENT_QUEUE_CAPACITY - 1)) == 0,
      "EVENT_QUEUE_CAPACITY must be a power of two");

  bool pushUnsafe(const EventRecord &record);
  bool pop(EventRecord &record);
  void writeRecord(const EventRecord &record);
  void writeOverflow(uint32_t count);

  EventRecord queue_[Config::EVENT_QUEUE_CAPACITY]{};
  volatile uint8_t head_ = 0;
  volatile uint8_t tail_ = 0;
  volatile uint16_t droppedCount_ = 0;
  uint16_t reportedDroppedCount_ = 0;
};
