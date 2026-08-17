#include "ServoActuator.h"

#include "MasterClock.h"

namespace {
constexpr uint32_t SERVO_UPDATE_PERIOD_US = 2000;

uint8_t clampAngle(uint8_t angle) {
  return angle > 180 ? 180 : angle;
}
}

ServoActuator::ServoActuator(
    uint8_t index,
    uint8_t pin,
    uint8_t homeAngle,
    HardwareScheduler &scheduler,
    EventLogger &logger)
    : index_(index),
      pin_(pin),
      homeAngle_(clampAngle(homeAngle)),
      scheduler_(scheduler),
      logger_(logger),
      currentAngle_(homeAngle_),
      startAngle_(homeAngle_),
      targetAngle_(homeAngle_) {}

void ServoActuator::begin() {
  (void)pin_;
  scheduler_.enableServo(index_, true);
  writeAngle(homeAngle_);
}

void ServoActuator::moveTo(uint8_t angle) {
  moveTo(angle, 0);
}

void ServoActuator::moveTo(uint8_t angle, uint32_t durationUs) {
  startSegment(angle, durationUs, State::MOVING);
}

void ServoActuator::moveToAndReturn(uint8_t angle, uint32_t travelUs, uint32_t holdUs) {
  holdDurationUs_ = holdUs;
  returnDurationUs_ = travelUs;
  startSegment(angle, travelUs, State::MOVING_OUT);
}

void ServoActuator::returnHome(uint32_t durationUs) {
  startSegment(homeAngle_, durationUs, State::MOVING_HOME);
}

void ServoActuator::startSegment(uint8_t targetAngle, uint32_t durationUs, State movingState) {
  targetAngle = clampAngle(targetAngle);
  durationUs = min(durationUs, 0x7FFFFFFFUL);
  const uint32_t nowUs = MasterClock::nowUs();
  startAngle_ = currentAngle_;
  targetAngle_ = targetAngle;
  moveStartedUs_ = nowUs;
  lastWriteUs_ = nowUs;
  moveDurationUs_ = durationUs;
  state_ = movingState;
  logger_.logAt(nowUs, startEvent(), 0, targetAngle_);

  if (durationUs == 0 || startAngle_ == targetAngle_) {
    currentAngle_ = targetAngle_;
    writeAngle(currentAngle_);
    finishSegment(nowUs);
  }
}

void ServoActuator::update(uint32_t nowUs) {
  if (state_ == State::IDLE) {
    return;
  }
  if (state_ == State::HOLDING) {
    if (MasterClock::elapsed(nowUs, holdStartedUs_, holdDurationUs_)) {
      startSegment(homeAngle_, returnDurationUs_, State::MOVING_HOME);
    }
    return;
  }

  if (MasterClock::elapsed(nowUs, moveStartedUs_, moveDurationUs_)) {
    currentAngle_ = targetAngle_;
    writeAngle(currentAngle_);
    finishSegment(nowUs);
    return;
  }

  if (!MasterClock::elapsed(nowUs, lastWriteUs_, SERVO_UPDATE_PERIOD_US)) {
    return;
  }
  lastWriteUs_ = nowUs;
  const uint32_t elapsedUs = static_cast<uint32_t>(nowUs - moveStartedUs_);
  const int32_t angleDelta = static_cast<int32_t>(targetAngle_) - startAngle_;
  const int32_t interpolated = static_cast<int32_t>(startAngle_) + static_cast<int32_t>(
      (static_cast<int64_t>(angleDelta) * elapsedUs) / moveDurationUs_);
  currentAngle_ = static_cast<uint8_t>(interpolated);
  writeAngle(currentAngle_);
}

void ServoActuator::writeAngle(uint8_t angle) {
  const uint32_t spanUs = Config::SERVO_MAX_PULSE_US - Config::SERVO_MIN_PULSE_US;
  const uint16_t pulseUs = static_cast<uint16_t>(
      Config::SERVO_MIN_PULSE_US + (spanUs * clampAngle(angle)) / 180UL);
  scheduler_.setServoPulseUs(index_, pulseUs);
}

void ServoActuator::finishSegment(uint32_t nowUs) {
  logger_.logAt(nowUs, stopEvent(), 0, currentAngle_);
  if (state_ == State::MOVING_OUT) {
    state_ = State::HOLDING;
    holdStartedUs_ = nowUs;
  } else {
    state_ = State::IDLE;
  }
}

void ServoActuator::stop() {
  if (state_ != State::IDLE) {
    state_ = State::IDLE;
    logger_.log(stopEvent(), 0, currentAngle_);
  }
}

void ServoActuator::setHome(uint8_t angle) {
  homeAngle_ = clampAngle(angle);
}

EventID ServoActuator::startEvent() const {
  return index_ == 0 ? EventID::SERVO_A_START : EventID::SERVO_B_START;
}

EventID ServoActuator::stopEvent() const {
  return index_ == 0 ? EventID::SERVO_A_STOP : EventID::SERVO_B_STOP;
}
