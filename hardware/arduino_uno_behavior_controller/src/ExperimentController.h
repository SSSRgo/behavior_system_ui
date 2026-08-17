#pragma once

#include <Arduino.h>

#include "EventLogger.h"
#include "ServoActuator.h"
#include "SyncManager.h"

enum class ExperimentState : uint8_t {
  IDLE = 0,
  ARMED = 1,
  TRIAL_START = 2,
  PRE_STIM = 3,
  STIMULUS = 4,
  POST_STIM = 5,
  ITI = 6,
  NEXT_TRIAL = 7,
};

struct ExperimentSettings {
  uint32_t brushDelayUs = 2000000;
  uint32_t brushDurationUs = 300000;
  uint32_t trialDurationUs = 10000000;
  uint32_t itiUs = 2000000;
  uint16_t trialCount = 1;
  uint32_t servoMoveUs = 150000;
  uint8_t servoHomeDeg = 90;
  uint8_t servoStimDeg = 120;
};

class ExperimentController {
 public:
  ExperimentController(EventLogger &logger, SyncManager &sync, ServoActuator &brushServo)
      : logger_(logger), sync_(sync), brushServo_(brushServo) {}

  void begin();
  void update(uint32_t nowUs);
  bool arm();
  bool start();
  void stop();
  bool setParameter(const char *name, uint32_t value);

  ExperimentState state() const { return state_; }
  const __FlashStringHelper *stateName() const;
  uint16_t trialNumber() const { return trialNumber_; }
  bool running() const;
  const ExperimentSettings &settings() const { return settings_; }

 private:
  void enterState(ExperimentState next, uint32_t nowUs);
  void beginTrial(uint32_t nowUs);
  void endSession(uint32_t nowUs, int32_t reason);
  bool settingsValid() const;

  EventLogger &logger_;
  SyncManager &sync_;
  ServoActuator &brushServo_;
  ExperimentSettings settings_{};
  ExperimentState state_ = ExperimentState::IDLE;
  uint32_t stateStartedUs_ = 0;
  uint32_t trialStartedUs_ = 0;
  volatile uint16_t trialNumber_ = 0;
};
