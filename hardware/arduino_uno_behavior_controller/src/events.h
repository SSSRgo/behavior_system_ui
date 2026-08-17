#pragma once

#include <Arduino.h>

enum class EventID : uint8_t {
  SESSION_START = 1,
  SESSION_END = 2,
  STATE_ENTER = 3,
  TRIAL_START = 10,
  TRIAL_END = 11,
  BRUSH_COMMAND = 20,
  BRUSH_CONTACT = 21,
  BRUSH_END = 22,
  MOTOR_A_START = 30,
  MOTOR_A_STOP = 31,
  MOTOR_B_START = 32,
  MOTOR_B_STOP = 33,
  SERVO_A_START = 40,
  SERVO_A_STOP = 41,
  SERVO_B_START = 42,
  SERVO_B_STOP = 43,
  REWARD_ON = 50,
  REWARD_OFF = 51,
  SENSOR_ON = 60,
  SENSOR_OFF = 61,
  LICK_ON = 62,
  LICK_OFF = 63,
  SYNC_OUT = 70,
  TWO_PHOTON_FRAME = 71,
  CAMERA_TRIGGER = 80,
  CAMERA_FRAME = 81,
  TEST_SYNC_START = 90,
  TEST_SYNC_PULSE = 91,
  TEST_SYNC_END = 92,
  EVENT_QUEUE_OVERFLOW = 100,
};

inline const __FlashStringHelper *eventIdName(EventID id) {
  // Explicit comparisons prevent avr-gcc from materializing a sparse 101-entry
  // pointer lookup table in the Uno's scarce SRAM.
  if (id == EventID::SESSION_START) return F("SESSION_START");
  if (id == EventID::SESSION_END) return F("SESSION_END");
  if (id == EventID::STATE_ENTER) return F("STATE_ENTER");
  if (id == EventID::TRIAL_START) return F("TRIAL_START");
  if (id == EventID::TRIAL_END) return F("TRIAL_END");
  if (id == EventID::BRUSH_COMMAND) return F("BRUSH_COMMAND");
  if (id == EventID::BRUSH_CONTACT) return F("BRUSH_CONTACT");
  if (id == EventID::BRUSH_END) return F("BRUSH_END");
  if (id == EventID::MOTOR_A_START) return F("MOTOR_A_START");
  if (id == EventID::MOTOR_A_STOP) return F("MOTOR_A_STOP");
  if (id == EventID::MOTOR_B_START) return F("MOTOR_B_START");
  if (id == EventID::MOTOR_B_STOP) return F("MOTOR_B_STOP");
  if (id == EventID::SERVO_A_START) return F("SERVO_A_START");
  if (id == EventID::SERVO_A_STOP) return F("SERVO_A_STOP");
  if (id == EventID::SERVO_B_START) return F("SERVO_B_START");
  if (id == EventID::SERVO_B_STOP) return F("SERVO_B_STOP");
  if (id == EventID::REWARD_ON) return F("REWARD_ON");
  if (id == EventID::REWARD_OFF) return F("REWARD_OFF");
  if (id == EventID::SENSOR_ON) return F("SENSOR_ON");
  if (id == EventID::SENSOR_OFF) return F("SENSOR_OFF");
  if (id == EventID::LICK_ON) return F("LICK_ON");
  if (id == EventID::LICK_OFF) return F("LICK_OFF");
  if (id == EventID::SYNC_OUT) return F("SYNC_OUT");
  if (id == EventID::TWO_PHOTON_FRAME) return F("TWO_PHOTON_FRAME");
  if (id == EventID::CAMERA_TRIGGER) return F("CAMERA_TRIGGER");
  if (id == EventID::CAMERA_FRAME) return F("CAMERA_FRAME");
  if (id == EventID::TEST_SYNC_START) return F("TEST_SYNC_START");
  if (id == EventID::TEST_SYNC_PULSE) return F("TEST_SYNC_PULSE");
  if (id == EventID::TEST_SYNC_END) return F("TEST_SYNC_END");
  if (id == EventID::EVENT_QUEUE_OVERFLOW) return F("EVENT_QUEUE_OVERFLOW");
  return F("UNKNOWN");
}
