#pragma once

#include <Arduino.h>

#include "EventLogger.h"
#include "SyncManager.h"

class SensorManager {
 public:
  SensorManager(EventLogger &logger, SyncManager &sync) : logger_(logger), sync_(sync) {}

  void begin(
      uint8_t brushContactPin,
      uint8_t lickPin,
      bool activeHigh,
      uint32_t debounceUs);
  void update(uint32_t nowUs);
  void setTrialNumber(uint16_t trialNumber) { trialNumber_ = trialNumber; }
  static void handlePinChangeInterrupt();

 private:
  struct Channel {
    uint8_t pin = 255;
    EventID onEvent = EventID::SENSOR_ON;
    EventID offEvent = EventID::SENSOR_OFF;
    volatile uint32_t lastEdgeUs = 0;
    volatile uint8_t *inputRegister = nullptr;
    uint8_t mask = 0;
    volatile bool lastLevel = false;
  };

  void handlePinChangesISR(uint32_t timestampUs);
  void handleEdgeISR(uint8_t channelIndex, uint32_t timestampUs);

  static SensorManager *instance_;
  EventLogger &logger_;
  SyncManager &sync_;
  Channel channels_[2]{};
  bool activeHigh_ = true;
  uint32_t debounceUs_ = 1000;
  volatile uint16_t trialNumber_ = 0;
};
