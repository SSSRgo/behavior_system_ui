#pragma once

#include <Arduino.h>

#include "CameraSync.h"
#include "EventLogger.h"
#include "ExperimentController.h"
#include "ServoActuator.h"
#include "StepperMotor.h"
#include "SyncManager.h"
#include "config.h"

class SerialCommandParser {
 public:
  SerialCommandParser(
      ExperimentController &experiment,
      StepperMotor &stepperA,
      StepperMotor &stepperB,
      ServoActuator &servoA,
      ServoActuator &servoB,
      SyncManager &sync,
      CameraSync &camera,
      EventLogger &logger);

  void begin(Stream &stream);
  void update();

 private:
  enum class SwingPhase : uint8_t { IDLE, OUTBOUND, RETURNING };

  void handleLine(char *line);
  void handleSet(char *savePtr);
  void handleMove(char *savePtr);
  void handleCamera(char *savePtr);
  void handleEnable(char *savePtr);
  void handleSwing(char *savePtr);
  void updateLegacySwing();
  bool startLegacySwing(bool repeat);
  void stopAll();
  void writeStatus();
  void acknowledge(const __FlashStringHelper *command);
  void error(const __FlashStringHelper *reason);

  ExperimentController &experiment_;
  StepperMotor &stepperA_;
  StepperMotor &stepperB_;
  ServoActuator &servoA_;
  ServoActuator &servoB_;
  SyncManager &sync_;
  CameraSync &camera_;
  EventLogger &logger_;
  Stream *stream_ = nullptr;
  char commandBuffer_[Config::SERIAL_COMMAND_BUFFER_SIZE]{};
  uint8_t commandLength_ = 0;
  uint32_t cameraPeriodUs_ = 33333;
  SwingPhase swingPhase_ = SwingPhase::IDLE;
  bool swingRepeat_ = false;
};
