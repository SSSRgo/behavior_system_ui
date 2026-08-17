#pragma once

#include <Arduino.h>

#include "EventLogger.h"

class SyncManager {
 public:
  explicit SyncManager(EventLogger &logger) : logger_(logger) {}

  void begin(uint8_t eventOutPin, uint8_t frameInPin, uint32_t pulseWidthUs);
  void update(uint32_t nowUs);
  void trigger(EventID sourceEvent, uint16_t trialNumber = 0);
  void triggerFromISR(uint32_t timestampUs, EventID sourceEvent, uint16_t trialNumber = 0);
  void setPulseWidthUs(uint32_t widthUs);

  void startTest(uint32_t pulseCount = 10, uint32_t intervalUs = 1000000);
  void stopTest();
  void simulateFrameEdge();
  void startFrameSimulation(uint32_t frameCount, uint32_t intervalUs);
  void stopFrameSimulation();

  uint32_t frameNumber() const { return frameNumber_; }
  bool pulseActive() const { return pulseActive_; }
  bool testActive() const { return testActive_; }
  bool frameSimulationActive() const { return frameSimulationActive_; }

 private:
  static void frameISRThunk();
  void handleFrameISR(uint32_t timestampUs);
  void startPulseUnsafe(uint32_t timestampUs);

  static SyncManager *instance_;
  EventLogger &logger_;
  uint8_t eventOutPin_ = 255;
  uint8_t frameInPin_ = 255;
  volatile uint8_t *eventOutPort_ = nullptr;
  uint8_t eventOutMask_ = 0;
  volatile bool pulseActive_ = false;
  volatile uint32_t pulseStartedUs_ = 0;
  uint32_t pulseWidthUs_ = 5000;
  volatile uint32_t frameNumber_ = 0;

  bool testActive_ = false;
  uint16_t testRequestedCount_ = 0;
  uint16_t testPulseCount_ = 0;
  uint32_t testIntervalUs_ = 1000000;
  uint32_t testNextUs_ = 0;
  bool frameSimulationActive_ = false;
  uint32_t simulatedFrameRequestedCount_ = 0;
  uint32_t simulatedFrameGeneratedCount_ = 0;
  uint32_t simulatedFrameIntervalUs_ = 33333;
  uint32_t simulatedFrameNextUs_ = 0;
};
