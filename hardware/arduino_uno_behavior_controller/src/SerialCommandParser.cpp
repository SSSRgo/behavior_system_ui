#include "SerialCommandParser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {
#define FLASH_EQUALS(text, literal) (strcmp_P((text), PSTR(literal)) == 0)
#define ACK_TEXT(literal) acknowledge(F(literal))
#define ERROR_TEXT(literal) error(F(literal))

constexpr uint32_t STEPS_PER_REV =
    Config::MOTOR_FULL_STEPS_PER_REV * Config::MOTOR_MICROSTEPS;
constexpr int32_t SWING_STEPS = static_cast<int32_t>((STEPS_PER_REV * 120UL + 180UL) / 360UL);

void uppercase(char *text) {
  for (; *text != '\0'; ++text) {
    *text = static_cast<char>(toupper(static_cast<unsigned char>(*text)));
  }
}

bool parseUnsigned(const char *text, uint32_t &value) {
  if (text == nullptr || *text == '\0' || *text == '-') {
    return false;
  }
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseSigned(const char *text, int32_t &value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  value = static_cast<int32_t>(parsed);
  return true;
}

bool parseFloatValue(const char *text, float &value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  value = static_cast<float>(strtod(text, &end));
  return end != text && *end == '\0';
}
}  // namespace

SerialCommandParser::SerialCommandParser(
    ExperimentController &experiment,
    StepperMotor &stepperA,
    StepperMotor &stepperB,
    ServoActuator &servoA,
    ServoActuator &servoB,
    SyncManager &sync,
    CameraSync &camera,
    EventLogger &logger)
    : experiment_(experiment),
      stepperA_(stepperA),
      stepperB_(stepperB),
      servoA_(servoA),
      servoB_(servoB),
      sync_(sync),
      camera_(camera),
      logger_(logger) {}

void SerialCommandParser::begin(Stream &stream) {
  stream_ = &stream;
  commandLength_ = 0;
}

void SerialCommandParser::update() {
  updateLegacySwing();
  if (stream_ == nullptr) {
    return;
  }

  for (uint8_t count = 0;
       count < Config::MAX_SERIAL_BYTES_PER_LOOP && stream_->available() > 0;
       ++count) {
    const char received = static_cast<char>(stream_->read());
    if (received == '\r') {
      continue;
    }
    if (received == '\n') {
      commandBuffer_[commandLength_] = '\0';
      if (commandLength_ > 0) {
        handleLine(commandBuffer_);
      }
      commandLength_ = 0;
      continue;
    }
    if (static_cast<size_t>(commandLength_) + 1U >= sizeof(commandBuffer_)) {
      commandLength_ = 0;
      ERROR_TEXT("COMMAND_TOO_LONG");
      continue;
    }
    commandBuffer_[commandLength_++] = received;
  }
}

void SerialCommandParser::handleLine(char *line) {
  // Preserve the verified one-letter Uno commands as non-blocking manual controls.
  if ((line[0] == 'f' || line[0] == 'F') && line[1] == '\0') {
    stepperA_.startMove(static_cast<int32_t>(STEPS_PER_REV));
    ACK_TEXT("F");
    return;
  }
  if ((line[0] == 'r' || line[0] == 'R') && line[1] == '\0') {
    stepperA_.startMove(-static_cast<int32_t>(STEPS_PER_REV));
    ACK_TEXT("R");
    return;
  }
  if ((line[0] == 'm' || line[0] == 'M') &&
      (isdigit(static_cast<unsigned char>(line[1])) || line[1] == '-' ||
       line[1] == '+' || line[1] == '.')) {
    float revolutions;
    if (parseFloatValue(line + 1, revolutions) &&
        stepperA_.startMove(static_cast<int32_t>(revolutions * STEPS_PER_REV))) {
      ACK_TEXT("M");
    } else {
      ERROR_TEXT("MOVE_REJECTED");
    }
    return;
  }
  if ((line[0] == 'v' || line[0] == 'V') && line[1] != '\0') {
    float rpm;
    if (parseFloatValue(line + 1, rpm) && rpm > 0.0f) {
      stepperA_.setSpeed(static_cast<uint32_t>(rpm * STEPS_PER_REV / 60.0f));
      ACK_TEXT("V");
    } else {
      ERROR_TEXT("BAD_RPM");
    }
    return;
  }
  if ((line[0] == 'o' || line[0] == 'O') && line[1] == '\0') {
    startLegacySwing(false) ? ACK_TEXT("O") : ERROR_TEXT("MOTOR_BUSY");
    return;
  }
  if ((line[0] == 'p' || line[0] == 'P') && line[1] == '\0') {
    startLegacySwing(true) ? ACK_TEXT("P") : ERROR_TEXT("MOTOR_BUSY");
    return;
  }
  if ((line[0] == 's' || line[0] == 'S') && line[1] == '\0') {
    swingRepeat_ = false;
    ACK_TEXT("S");
    return;
  }
  if ((line[0] == 'e' || line[0] == 'E') && line[1] == '\0') {
    stepperA_.setEnabled(!stepperA_.isEnabled());
    ACK_TEXT("E");
    return;
  }
  if ((line[0] == 'c' || line[0] == 'C') && line[1] == '\0') {
    ERROR_TEXT("DRIVER_CONFIG_NOT_IMPLEMENTED_USE_HARDWARE_UART");
    return;
  }

  uppercase(line);
  char *savePtr = nullptr;
  const char *command = strtok_r(line, " \t", &savePtr);
  if (command == nullptr) {
    return;
  }

  if (FLASH_EQUALS(command, "ARM")) {
    experiment_.arm() ? ACK_TEXT("ARM") : ERROR_TEXT("ARM_REJECTED");
  } else if (FLASH_EQUALS(command, "START")) {
    experiment_.start() ? ACK_TEXT("START") : ERROR_TEXT("START_REQUIRES_ARMED_VALID_CONFIG");
  } else if (FLASH_EQUALS(command, "STOP")) {
    stopAll();
    ACK_TEXT("STOP");
  } else if (FLASH_EQUALS(command, "STATUS")) {
    writeStatus();
  } else if (FLASH_EQUALS(command, "SET")) {
    handleSet(savePtr);
  } else if (FLASH_EQUALS(command, "MOVE")) {
    handleMove(savePtr);
  } else if (FLASH_EQUALS(command, "CAMERA")) {
    handleCamera(savePtr);
  } else if (FLASH_EQUALS(command, "ENABLE")) {
    handleEnable(savePtr);
  } else if (FLASH_EQUALS(command, "SWING")) {
    handleSwing(savePtr);
  } else if (FLASH_EQUALS(command, "TEST_SYNC")) {
    uint32_t count = 10;
    uint32_t intervalUs = 1000000;
    const char *countText = strtok_r(nullptr, " \t", &savePtr);
    const char *intervalText = strtok_r(nullptr, " \t", &savePtr);
    if ((countText != nullptr && !parseUnsigned(countText, count)) ||
        (intervalText != nullptr && !parseUnsigned(intervalText, intervalUs))) {
      ERROR_TEXT("BAD_TEST_SYNC_ARGUMENTS");
    } else {
      sync_.startTest(count, intervalUs);
      ACK_TEXT("TEST_SYNC");
    }
  } else if (FLASH_EQUALS(command, "SIM_FRAME")) {
    uint32_t count = 1;
    uint32_t intervalUs = 33333;
    const char *countText = strtok_r(nullptr, " \t", &savePtr);
    const char *intervalText = strtok_r(nullptr, " \t", &savePtr);
    if ((countText != nullptr && !parseUnsigned(countText, count)) ||
        (intervalText != nullptr && !parseUnsigned(intervalText, intervalUs))) {
      ERROR_TEXT("BAD_SIM_FRAME_ARGUMENTS");
    } else {
      sync_.startFrameSimulation(count, intervalUs);
      ACK_TEXT("SIM_FRAME");
    }
  } else if (FLASH_EQUALS(command, "HOME")) {
    ERROR_TEXT("HOME_REQUIRES_LIMIT_OR_HOME_SENSOR");
  } else {
    ERROR_TEXT("UNKNOWN_COMMAND");
  }
}

void SerialCommandParser::handleSet(char *savePtr) {
  char *name = strtok_r(nullptr, " \t", &savePtr);
  const char *valueText = strtok_r(nullptr, " \t", &savePtr);
  uint32_t value;
  if (name == nullptr || !parseUnsigned(valueText, value)) {
    ERROR_TEXT("SET_REQUIRES_NAME_AND_UNSIGNED_VALUE");
    return;
  }

  bool accepted = experiment_.setParameter(name, value);
  if (FLASH_EQUALS(name, "EVENT_PULSE_US")) {
    sync_.setPulseWidthUs(value);
    accepted = true;
  } else if (FLASH_EQUALS(name, "STEPPER_A_SPEED_SPS")) {
    stepperA_.setSpeed(value);
    accepted = true;
  } else if (FLASH_EQUALS(name, "STEPPER_B_SPEED_SPS")) {
    stepperB_.setSpeed(value);
    accepted = true;
  } else if (FLASH_EQUALS(name, "CAMERA_PERIOD_US")) {
    cameraPeriodUs_ = value;
    accepted = true;
  }

  accepted ? ACK_TEXT("SET") : ERROR_TEXT("SET_REJECTED_OR_UNKNOWN_PARAMETER");
}

void SerialCommandParser::handleMove(char *savePtr) {
  char *actuator = strtok_r(nullptr, " \t", &savePtr);
  const char *valueText = strtok_r(nullptr, " \t", &savePtr);
  const char *durationOrSpeedText = strtok_r(nullptr, " \t", &savePtr);
  if (actuator == nullptr || valueText == nullptr) {
    ERROR_TEXT("MOVE_REQUIRES_ACTUATOR_AND_VALUE");
    return;
  }

  if (FLASH_EQUALS(actuator, "STEPPER_A") || FLASH_EQUALS(actuator, "STEPPER_B")) {
    int32_t steps;
    const bool isStepperA = FLASH_EQUALS(actuator, "STEPPER_A");
    uint32_t speed = isStepperA ? stepperA_.speed() : stepperB_.speed();
    if (!parseSigned(valueText, steps) ||
        (durationOrSpeedText != nullptr && !parseUnsigned(durationOrSpeedText, speed))) {
      ERROR_TEXT("BAD_STEPPER_MOVE");
      return;
    }
    const bool accepted = isStepperA ? stepperA_.startMove(steps, speed)
                                     : stepperB_.startMove(steps, speed);
    accepted ? ACK_TEXT("MOVE") : ERROR_TEXT("MOTOR_BUSY_OR_ZERO_MOVE");
    return;
  }

  if (FLASH_EQUALS(actuator, "STEPPER_A_REV") || FLASH_EQUALS(actuator, "MOTOR_A")) {
    float revolutions;
    if (!parseFloatValue(valueText, revolutions) ||
        !stepperA_.startMove(static_cast<int32_t>(revolutions * STEPS_PER_REV))) {
      ERROR_TEXT("BAD_OR_REJECTED_REVOLUTION_MOVE");
    } else {
      ACK_TEXT("MOVE");
    }
    return;
  }

  if (FLASH_EQUALS(actuator, "SERVO_A") || FLASH_EQUALS(actuator, "SERVO_B")) {
    uint32_t angle;
    uint32_t durationUs = 0;
    if (!parseUnsigned(valueText, angle) || angle > 180 ||
        (durationOrSpeedText != nullptr && !parseUnsigned(durationOrSpeedText, durationUs))) {
      ERROR_TEXT("BAD_SERVO_MOVE");
      return;
    }
    if (FLASH_EQUALS(actuator, "SERVO_A")) {
      servoA_.moveTo(static_cast<uint8_t>(angle), durationUs);
    } else {
      servoB_.moveTo(static_cast<uint8_t>(angle), durationUs);
    }
    ACK_TEXT("MOVE");
    return;
  }

  ERROR_TEXT("UNKNOWN_ACTUATOR");
}

void SerialCommandParser::handleCamera(char *savePtr) {
  const char *operation = strtok_r(nullptr, " \t", &savePtr);
  if (operation == nullptr) {
    ERROR_TEXT("CAMERA_REQUIRES_OPERATION");
  } else if (FLASH_EQUALS(operation, "SINGLE")) {
    camera_.singleTrigger();
    ACK_TEXT("CAMERA");
  } else if (FLASH_EQUALS(operation, "START")) {
    uint32_t periodUs = cameraPeriodUs_;
    const char *periodText = strtok_r(nullptr, " \t", &savePtr);
    if (periodText != nullptr && !parseUnsigned(periodText, periodUs)) {
      ERROR_TEXT("BAD_CAMERA_PERIOD");
      return;
    }
    camera_.startTriggerTrain(periodUs);
    ACK_TEXT("CAMERA");
  } else if (FLASH_EQUALS(operation, "STOP")) {
    camera_.stopTriggerTrain();
    ACK_TEXT("CAMERA");
  } else {
    ERROR_TEXT("UNKNOWN_CAMERA_OPERATION");
  }
}

void SerialCommandParser::handleEnable(char *savePtr) {
  char *actuator = strtok_r(nullptr, " \t", &savePtr);
  const char *state = strtok_r(nullptr, " \t", &savePtr);
  if (actuator == nullptr || state == nullptr ||
      (!FLASH_EQUALS(actuator, "STEPPER_A") && !FLASH_EQUALS(actuator, "STEPPER_B"))) {
    ERROR_TEXT("BAD_ENABLE_COMMAND");
    return;
  }
  StepperMotor &motor = FLASH_EQUALS(actuator, "STEPPER_A") ? stepperA_ : stepperB_;
  if (FLASH_EQUALS(state, "TOGGLE")) {
    motor.setEnabled(!motor.isEnabled());
  } else if (FLASH_EQUALS(state, "1") || FLASH_EQUALS(state, "ON")) {
    motor.setEnabled(true);
  } else if (FLASH_EQUALS(state, "0") || FLASH_EQUALS(state, "OFF")) {
    motor.setEnabled(false);
  } else {
    ERROR_TEXT("ENABLE_STATE_MUST_BE_ON_OFF_OR_TOGGLE");
    return;
  }
  ACK_TEXT("ENABLE");
}

void SerialCommandParser::handleSwing(char *savePtr) {
  const char *operation = strtok_r(nullptr, " \t", &savePtr);
  if (operation == nullptr) {
    ERROR_TEXT("SWING_REQUIRES_OPERATION");
  } else if (FLASH_EQUALS(operation, "ONCE")) {
    startLegacySwing(false) ? ACK_TEXT("SWING") : ERROR_TEXT("MOTOR_BUSY");
  } else if (FLASH_EQUALS(operation, "START")) {
    startLegacySwing(true) ? ACK_TEXT("SWING") : ERROR_TEXT("MOTOR_BUSY");
  } else if (FLASH_EQUALS(operation, "STOP")) {
    swingRepeat_ = false;
    ACK_TEXT("SWING");
  } else {
    ERROR_TEXT("UNKNOWN_SWING_OPERATION");
  }
}

bool SerialCommandParser::startLegacySwing(bool repeat) {
  if (stepperA_.isBusy() || swingPhase_ != SwingPhase::IDLE) {
    return false;
  }
  swingRepeat_ = repeat;
  if (!stepperA_.startMove(SWING_STEPS)) {
    return false;
  }
  sync_.trigger(EventID::MOTOR_A_START, 0);
  swingPhase_ = SwingPhase::OUTBOUND;
  return true;
}

void SerialCommandParser::updateLegacySwing() {
  if (swingPhase_ == SwingPhase::OUTBOUND && !stepperA_.isBusy()) {
    if (stepperA_.startMove(-SWING_STEPS)) {
      sync_.trigger(EventID::MOTOR_A_START, 0);
      swingPhase_ = SwingPhase::RETURNING;
    }
  } else if (swingPhase_ == SwingPhase::RETURNING && !stepperA_.isBusy()) {
    swingPhase_ = SwingPhase::IDLE;
    if (swingRepeat_) {
      startLegacySwing(true);
    }
  }
}

void SerialCommandParser::stopAll() {
  experiment_.stop();
  swingRepeat_ = false;
  swingPhase_ = SwingPhase::IDLE;
  stepperA_.stop();
  stepperB_.stop();
  servoA_.returnHome();
  servoB_.returnHome();
  camera_.stopTriggerTrain();
  sync_.stopTest();
  sync_.stopFrameSimulation();
}

void SerialCommandParser::writeStatus() {
  if (stream_ == nullptr) {
    return;
  }
  stream_->print(F("STATUS,"));
  stream_->print(experiment_.stateName());
  stream_->print(F(",TRIAL,"));
  stream_->print(experiment_.trialNumber());
  stream_->print(F(",QUEUE,"));
  stream_->print(logger_.queuedCount());
  stream_->print(F(",FRAMES,"));
  stream_->println(sync_.frameNumber());
}

void SerialCommandParser::acknowledge(const __FlashStringHelper *command) {
  if (stream_ != nullptr) {
    stream_->print(F("ACK,"));
    stream_->println(command);
  }
}

void SerialCommandParser::error(const __FlashStringHelper *reason) {
  if (stream_ != nullptr) {
    stream_->print(F("ERR,"));
    stream_->println(reason);
  }
}
