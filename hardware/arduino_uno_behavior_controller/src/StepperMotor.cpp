#include "StepperMotor.h"

#include "MasterClock.h"
#include "config.h"

StepperMotor::StepperMotor(
    uint8_t index,
    StepperPins pins,
    HardwareScheduler &scheduler,
    EventLogger &logger)
    : index_(index), pins_(pins), scheduler_(scheduler), logger_(logger) {}

void StepperMotor::begin() {
  pinMode(pins_.step, OUTPUT);
  pinMode(pins_.direction, OUTPUT);
  pinMode(pins_.enable, OUTPUT);
  digitalWrite(pins_.step, outputLevel(false));
  digitalWrite(pins_.direction, outputLevel(false));
  setEnabled(false);
}

bool StepperMotor::startMove(int32_t steps) {
  return startMove(steps, speedStepsPerSecond_);
}

bool StepperMotor::startMove(int32_t steps, uint32_t speedStepsPerSecond) {
  if (steps == 0 || isBusy() || index_ >= 2) {
    return false;
  }

  setSpeed(speedStepsPerSecond);
  setEnabled(true);
  setDirection(steps > 0);

  const uint32_t pulseCount = static_cast<uint32_t>(steps > 0 ? steps : -static_cast<int64_t>(steps));
  if (!scheduler_.startStepper(index_, pulseCount, speedStepsPerSecond_, directionSign_)) {
    return false;
  }
  logger_.log(startEvent(), 0, steps);
  return true;
}

void StepperMotor::stop() {
  const bool wasBusy = isBusy();
  scheduler_.stopStepper(index_);
  if (wasBusy) {
    logger_.log(stopEvent(), 0, positionSteps());
  }
}

void StepperMotor::update(uint32_t nowUs) {
  (void)nowUs;
  if (!scheduler_.takeStepperCompletion(index_)) {
    return;
  }
  logger_.log(stopEvent(), 0, positionSteps());
}

void StepperMotor::setSpeed(uint32_t speedStepsPerSecond) {
  speedStepsPerSecond_ = constrain(
      speedStepsPerSecond, 1UL, Config::MAX_STEPPER_SPEED_SPS);
}

void StepperMotor::setDirection(bool forward) {
  directionSign_ = forward ? 1 : -1;
  digitalWrite(pins_.direction, outputLevel(forward));
}

void StepperMotor::setEnabled(bool enabled) {
  enabled_ = enabled;
  digitalWrite(pins_.enable, outputLevel(enabled));
}

int StepperMotor::outputLevel(bool active) const {
  return pins_.activeHigh == active ? HIGH : LOW;
}

EventID StepperMotor::startEvent() const {
  return index_ == 0 ? EventID::MOTOR_A_START : EventID::MOTOR_B_START;
}

EventID StepperMotor::stopEvent() const {
  return index_ == 0 ? EventID::MOTOR_A_STOP : EventID::MOTOR_B_STOP;
}
