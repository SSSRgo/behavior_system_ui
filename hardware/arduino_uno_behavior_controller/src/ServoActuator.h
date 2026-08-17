#pragma once

#include <Arduino.h>

#include "EventLogger.h"
#include "HardwareScheduler.h"

class ServoActuator {
 public:
  ServoActuator(
      uint8_t index,
      uint8_t pin,
      uint8_t homeAngle,
      HardwareScheduler &scheduler,
      EventLogger &logger);

  void begin();
  void update(uint32_t nowUs);
  void moveTo(uint8_t angle);
  void moveTo(uint8_t angle, uint32_t durationUs);
  void moveToAndReturn(uint8_t angle, uint32_t travelUs, uint32_t holdUs);
  void returnHome(uint32_t durationUs = 0);
  void stop();
  void setHome(uint8_t angle);

  bool isBusy() const { return state_ != State::IDLE; }
  uint8_t currentAngle() const { return currentAngle_; }
  uint8_t homeAngle() const { return homeAngle_; }

 private:
  enum class State : uint8_t { IDLE, MOVING, MOVING_OUT, HOLDING, MOVING_HOME };

  void startSegment(uint8_t targetAngle, uint32_t durationUs, State movingState);
  void finishSegment(uint32_t nowUs);
  void writeAngle(uint8_t angle);
  EventID startEvent() const;
  EventID stopEvent() const;

  uint8_t index_;
  uint8_t pin_;
  uint8_t homeAngle_;
  HardwareScheduler &scheduler_;
  EventLogger &logger_;
  State state_ = State::IDLE;
  uint8_t currentAngle_;
  uint8_t startAngle_;
  uint8_t targetAngle_;
  uint32_t moveStartedUs_ = 0;
  uint32_t moveDurationUs_ = 0;
  uint32_t lastWriteUs_ = 0;
  uint32_t holdStartedUs_ = 0;
  uint32_t holdDurationUs_ = 0;
  uint32_t returnDurationUs_ = 0;
};
