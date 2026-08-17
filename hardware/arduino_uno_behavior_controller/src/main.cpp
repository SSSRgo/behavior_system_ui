#include <Arduino.h>

#include "CameraSync.h"
#include "EventLogger.h"
#include "ExperimentController.h"
#include "HardwareScheduler.h"
#include "MasterClock.h"
#include "SensorManager.h"
#include "SerialCommandParser.h"
#include "ServoActuator.h"
#include "StepperMotor.h"
#include "SyncManager.h"
#include "config.h"

EventLogger logger;
SyncManager syncManager(logger);
HardwareScheduler hardwareScheduler;

StepperMotor stepperA(
    0,
    StepperPins{
        Config::STEPPER_A_STEP_PIN,
        Config::STEPPER_A_DIR_PIN,
        Config::STEPPER_A_ENABLE_PIN,
        Config::STEPPER_INPUT_ACTIVE_HIGH},
    hardwareScheduler,
    logger);
StepperMotor stepperB(
    1,
    StepperPins{
        Config::STEPPER_B_STEP_PIN,
        Config::STEPPER_B_DIR_PIN,
        Config::STEPPER_B_ENABLE_PIN,
        Config::STEPPER_INPUT_ACTIVE_HIGH},
    hardwareScheduler,
    logger);

ServoActuator servoA(
    0,
    Config::SERVO_A_PIN,
    Config::SERVO_A_HOME_DEG,
    hardwareScheduler,
    logger);
ServoActuator servoB(
    1,
    Config::SERVO_B_PIN,
    Config::SERVO_B_HOME_DEG,
    hardwareScheduler,
    logger);
CameraSync cameraSync(logger);
SensorManager sensorManager(logger, syncManager);
ExperimentController experiment(logger, syncManager, servoA);
SerialCommandParser serialParser(
    experiment,
    stepperA,
    stepperB,
    servoA,
    servoB,
    syncManager,
    cameraSync,
    logger);

void setup() {
  pinMode(Config::STATUS_LED_PIN, OUTPUT);
  digitalWrite(Config::STATUS_LED_PIN, LOW);
  pinMode(Config::REWARD_VALVE_OUT_PIN, OUTPUT);
  digitalWrite(Config::REWARD_VALVE_OUT_PIN, LOW);

  Serial.begin(Config::SERIAL_BAUD);
  logger.begin();
  hardwareScheduler.begin(
      Config::STEPPER_A_STEP_PIN,
      Config::STEPPER_B_STEP_PIN,
      Config::SERVO_A_PIN,
      Config::SERVO_B_PIN);
  stepperA.begin();
  stepperB.begin();
  servoA.begin();
  servoB.begin();
  syncManager.begin(
      Config::EVENT_TTL_OUT_PIN,
      Config::TWO_PHOTON_FRAME_IN_PIN,
      Config::EVENT_PULSE_WIDTH_US);
  cameraSync.begin(
      Config::CAMERA_TRIGGER_OUT_PIN,
      Config::CAMERA_FRAME_IN_PIN,
      Config::CAMERA_TRIGGER_WIDTH_US);
  sensorManager.begin(
      Config::BRUSH_CONTACT_IN_PIN,
      Config::LICK_SENSOR_IN_PIN,
      Config::SENSOR_ACTIVE_HIGH,
      Config::SENSOR_DEBOUNCE_US);
  experiment.begin();
  serialParser.begin(Serial);

  Serial.println(F("STATUS,READY"));
}

void loop() {
  const uint32_t nowUs = MasterClock::nowUs();

  experiment.update(nowUs);

  stepperA.update(nowUs);
  stepperB.update(nowUs);
  servoA.update(nowUs);
  servoB.update(nowUs);

  syncManager.update(nowUs);
  cameraSync.update(nowUs);

  sensorManager.setTrialNumber(experiment.trialNumber());
  sensorManager.update(nowUs);

  serialParser.update();
  logger.flush();
}
