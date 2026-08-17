#pragma once

#include <Arduino.h>

#include "EventLogger.h"
#include "HardwareScheduler.h"

struct StepperPins {
  uint8_t step;
  uint8_t direction;
  uint8_t enable;
  bool activeHigh;
};

class StepperMotor {
 public:
  StepperMotor(
      uint8_t index,
      StepperPins pins,
      HardwareScheduler &scheduler,
      EventLogger &logger);

  void begin();
  bool startMove(int32_t steps);
  bool startMove(int32_t steps, uint32_t speedStepsPerSecond);
  void stop();
  void update(uint32_t nowUs);
  void setSpeed(uint32_t speedStepsPerSecond);
  void setDirection(bool forward);
  void setEnabled(bool enabled);

  bool isBusy() const { return scheduler_.stepperBusy(index_); }
  bool isEnabled() const { return enabled_; }
  uint32_t speed() const { return speedStepsPerSecond_; }
  int32_t positionSteps() const { return scheduler_.stepperPosition(index_); }

 private:
  int outputLevel(bool active) const;
  EventID startEvent() const;
  EventID stopEvent() const;

  uint8_t index_;
  StepperPins pins_;
  HardwareScheduler &scheduler_;
  EventLogger &logger_;
  int8_t directionSign_ = 1;
  bool enabled_ = false;
  uint32_t speedStepsPerSecond_ = 3200;
};
