#include "EventLogger.h"

#include "MasterClock.h"

namespace {
constexpr uint8_t QUEUE_MASK = Config::EVENT_QUEUE_CAPACITY - 1;
constexpr int MIN_SERIAL_SPACE = 60;
}  // namespace

void EventLogger::begin() {
  noInterrupts();
  head_ = 0;
  tail_ = 0;
  droppedCount_ = 0;
  interrupts();
  reportedDroppedCount_ = 0;
}

bool EventLogger::log(EventID eventId, uint16_t trialNumber, int32_t value) {
  return logAt(MasterClock::nowUs(), eventId, trialNumber, value);
}

bool EventLogger::logAt(uint32_t timestampUs, EventID eventId, uint16_t trialNumber, int32_t value) {
  const EventRecord record{timestampUs, trialNumber, value, eventId};
  noInterrupts();
  const bool pushed = pushUnsafe(record);
  interrupts();
  return pushed;
}

bool EventLogger::logFromISR(
    uint32_t timestampUs, EventID eventId, uint16_t trialNumber, int32_t value) {
  return pushUnsafe(EventRecord{timestampUs, trialNumber, value, eventId});
}

bool EventLogger::pushUnsafe(const EventRecord &record) {
  const uint8_t next = (head_ + 1U) & QUEUE_MASK;
  if (next == tail_) {
    if (droppedCount_ != UINT16_MAX) {
      ++droppedCount_;
    }
    return false;
  }
  queue_[head_] = record;
  head_ = next;
  return true;
}

bool EventLogger::pop(EventRecord &record) {
  noInterrupts();
  if (tail_ == head_) {
    interrupts();
    return false;
  }
  record = queue_[tail_];
  tail_ = (tail_ + 1U) & QUEUE_MASK;
  interrupts();
  return true;
}

uint8_t EventLogger::queuedCount() const {
  noInterrupts();
  const uint8_t count = (head_ - tail_) & QUEUE_MASK;
  interrupts();
  return count;
}

void EventLogger::flush(uint8_t maxRecords) {
  uint16_t droppedSnapshot;
  noInterrupts();
  droppedSnapshot = droppedCount_;
  interrupts();

  if (droppedSnapshot != reportedDroppedCount_ && Serial.availableForWrite() >= MIN_SERIAL_SPACE) {
    writeOverflow(droppedSnapshot - reportedDroppedCount_);
    reportedDroppedCount_ = droppedSnapshot;
  }

  EventRecord record{};
  for (uint8_t i = 0; i < maxRecords; ++i) {
    if (Serial.availableForWrite() < MIN_SERIAL_SPACE || !pop(record)) {
      return;
    }
    writeRecord(record);
  }
}

void EventLogger::writeRecord(const EventRecord &record) {
  if (record.eventId == EventID::TWO_PHOTON_FRAME) {
    Serial.print(F("FRAME,"));
    Serial.print(static_cast<uint32_t>(record.value));
    Serial.print(',');
    Serial.println(record.timestampUs);
  } else {
    Serial.print(F("EVENT,"));
    Serial.print(record.timestampUs);
    Serial.print(',');
    Serial.print(record.trialNumber);
    Serial.print(',');
    Serial.print(eventIdName(record.eventId));
    Serial.print(',');
    Serial.println(record.value);
  }
}

void EventLogger::writeOverflow(uint32_t count) {
  const EventRecord record{
      MasterClock::nowUs(), 0, static_cast<int32_t>(count), EventID::EVENT_QUEUE_OVERFLOW};
  writeRecord(record);
}
