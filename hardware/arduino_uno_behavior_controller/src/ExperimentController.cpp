#include "ExperimentController.h"

#include <string.h>

#include "MasterClock.h"

void ExperimentController::begin() {
  state_ = ExperimentState::IDLE;
  stateStartedUs_ = MasterClock::nowUs();
  trialNumber_ = 0;
}

bool ExperimentController::arm() {
  if (state_ != ExperimentState::IDLE || !settingsValid()) {
    return false;
  }
  enterState(ExperimentState::ARMED, MasterClock::nowUs());
  return true;
}

bool ExperimentController::start() {
  if (state_ != ExperimentState::ARMED || !settingsValid()) {
    return false;
  }
  const uint32_t nowUs = MasterClock::nowUs();
  trialNumber_ = 0;
  logger_.logAt(nowUs, EventID::SESSION_START, 0, 1);
  enterState(ExperimentState::TRIAL_START, nowUs);
  return true;
}

void ExperimentController::stop() {
  if (state_ == ExperimentState::IDLE) {
    return;
  }
  if (state_ == ExperimentState::ARMED) {
    enterState(ExperimentState::IDLE, MasterClock::nowUs());
    return;
  }
  endSession(MasterClock::nowUs(), -1);
}

bool ExperimentController::running() const {
  return state_ != ExperimentState::IDLE && state_ != ExperimentState::ARMED;
}

void ExperimentController::update(uint32_t nowUs) {
  switch (state_) {
    case ExperimentState::IDLE:
    case ExperimentState::ARMED:
      return;

    case ExperimentState::TRIAL_START:
      beginTrial(nowUs);
      enterState(ExperimentState::PRE_STIM, nowUs);
      return;

    case ExperimentState::PRE_STIM:
      if (MasterClock::elapsed(nowUs, trialStartedUs_, settings_.brushDelayUs)) {
        logger_.logAt(
            nowUs, EventID::BRUSH_COMMAND, trialNumber_, settings_.servoStimDeg);
        sync_.trigger(EventID::BRUSH_COMMAND, trialNumber_);
        brushServo_.moveTo(settings_.servoStimDeg, settings_.servoMoveUs);
        enterState(ExperimentState::STIMULUS, nowUs);
      }
      return;

    case ExperimentState::STIMULUS:
      if (MasterClock::elapsed(
              nowUs,
              trialStartedUs_,
              settings_.brushDelayUs + settings_.brushDurationUs)) {
        brushServo_.returnHome(settings_.servoMoveUs);
        logger_.logAt(nowUs, EventID::BRUSH_END, trialNumber_, settings_.servoHomeDeg);
        enterState(ExperimentState::POST_STIM, nowUs);
      }
      return;

    case ExperimentState::POST_STIM:
      if (MasterClock::elapsed(nowUs, trialStartedUs_, settings_.trialDurationUs)) {
        logger_.logAt(nowUs, EventID::TRIAL_END, trialNumber_, 0);
        enterState(ExperimentState::ITI, nowUs);
      }
      return;

    case ExperimentState::ITI:
      if (MasterClock::elapsed(nowUs, stateStartedUs_, settings_.itiUs)) {
        enterState(ExperimentState::NEXT_TRIAL, nowUs);
      }
      return;

    case ExperimentState::NEXT_TRIAL:
      if (trialNumber_ >= settings_.trialCount) {
        endSession(nowUs, 0);
      } else {
        enterState(ExperimentState::TRIAL_START, nowUs);
      }
      return;
  }
}

void ExperimentController::beginTrial(uint32_t nowUs) {
  ++trialNumber_;
  trialStartedUs_ = nowUs;
  logger_.logAt(nowUs, EventID::TRIAL_START, trialNumber_, 1);
  sync_.trigger(EventID::TRIAL_START, trialNumber_);
}

void ExperimentController::endSession(uint32_t nowUs, int32_t reason) {
  brushServo_.returnHome(settings_.servoMoveUs);
  logger_.logAt(nowUs, EventID::SESSION_END, trialNumber_, reason);
  enterState(ExperimentState::IDLE, nowUs);
}

void ExperimentController::enterState(ExperimentState next, uint32_t nowUs) {
  state_ = next;
  stateStartedUs_ = nowUs;
  logger_.logAt(nowUs, EventID::STATE_ENTER, trialNumber_, static_cast<int32_t>(next));
}

bool ExperimentController::setParameter(const char *name, uint32_t value) {
  if (state_ != ExperimentState::IDLE) {
    return false;
  }
  if (strcmp_P(name, PSTR("BRUSH_DELAY_US")) == 0) {
    settings_.brushDelayUs = value;
  } else if (strcmp_P(name, PSTR("BRUSH_DURATION_US")) == 0) {
    settings_.brushDurationUs = value;
  } else if (strcmp_P(name, PSTR("TRIAL_DURATION_US")) == 0) {
    settings_.trialDurationUs = value;
  } else if (strcmp_P(name, PSTR("ITI_US")) == 0) {
    settings_.itiUs = value;
  } else if (strcmp_P(name, PSTR("TRIAL_COUNT")) == 0) {
    settings_.trialCount = static_cast<uint16_t>(constrain(value, 1UL, 65535UL));
  } else if (strcmp_P(name, PSTR("SERVO_MOVE_US")) == 0) {
    settings_.servoMoveUs = value;
  } else if (strcmp_P(name, PSTR("SERVO_A_HOME")) == 0 && value <= 180) {
    settings_.servoHomeDeg = static_cast<uint8_t>(value);
    brushServo_.setHome(settings_.servoHomeDeg);
  } else if (strcmp_P(name, PSTR("SERVO_A_STIM")) == 0 && value <= 180) {
    settings_.servoStimDeg = static_cast<uint8_t>(value);
  } else {
    return false;
  }
  return true;
}

bool ExperimentController::settingsValid() const {
  const uint64_t stimulusEnd =
      static_cast<uint64_t>(settings_.brushDelayUs) + settings_.brushDurationUs;
  return settings_.trialCount > 0 && stimulusEnd <= settings_.trialDurationUs &&
         settings_.brushDelayUs < 0x80000000UL && settings_.brushDurationUs < 0x80000000UL &&
         settings_.trialDurationUs < 0x80000000UL && settings_.itiUs < 0x80000000UL;
}

const __FlashStringHelper *ExperimentController::stateName() const {
  if (state_ == ExperimentState::IDLE) return F("IDLE");
  if (state_ == ExperimentState::ARMED) return F("ARMED");
  if (state_ == ExperimentState::TRIAL_START) return F("TRIAL_START");
  if (state_ == ExperimentState::PRE_STIM) return F("PRE_STIM");
  if (state_ == ExperimentState::STIMULUS) return F("STIMULUS");
  if (state_ == ExperimentState::POST_STIM) return F("POST_STIM");
  if (state_ == ExperimentState::ITI) return F("ITI");
  if (state_ == ExperimentState::NEXT_TRIAL) return F("NEXT_TRIAL");
  return F("UNKNOWN");
}
