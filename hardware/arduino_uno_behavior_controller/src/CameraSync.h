#pragma once

#include <Arduino.h>

#include "EventLogger.h"

class CameraSync {
 public:
  explicit CameraSync(EventLogger &logger) : logger_(logger) {}

  void begin(uint8_t triggerOutPin, uint8_t frameInPin, uint32_t pulseWidthUs);
  void update(uint32_t nowUs);
  void singleTrigger();
  void startTriggerTrain(uint32_t periodUs);
  void stopTriggerTrain();

  uint32_t frameNumber() const { return frameNumber_; }
  bool trainActive() const { return trainActive_; }

 private:
  static void frameISRThunk();
  void handleFrameISR(uint32_t timestampUs);
  void startPulse(uint32_t timestampUs);

  static CameraSync *instance_;
  EventLogger &logger_;
  uint8_t triggerOutPin_ = 255;
  uint8_t frameInPin_ = 255;
  uint32_t pulseWidthUs_ = 1000;
  bool pulseActive_ = false;
  uint32_t pulseStartedUs_ = 0;
  bool trainActive_ = false;
  uint32_t trainPeriodUs_ = 33333;
  uint32_t nextTriggerUs_ = 0;
  volatile uint32_t frameNumber_ = 0;
};

