#pragma once

#include <Arduino.h>

namespace Config {

constexpr uint32_t SERIAL_BAUD = 115200;

// D0/D1 stay reserved for USB serial. D2/D3 are the Uno's external interrupts.
constexpr uint8_t TWO_PHOTON_FRAME_IN_PIN = 2;
constexpr uint8_t CAMERA_FRAME_IN_PIN = 3;

constexpr uint8_t STEPPER_A_STEP_PIN = 4;
constexpr uint8_t STEPPER_A_DIR_PIN = 5;
constexpr uint8_t STEPPER_A_ENABLE_PIN = 6;
constexpr uint8_t STEPPER_B_STEP_PIN = 7;
constexpr uint8_t STEPPER_B_DIR_PIN = 8;
constexpr uint8_t STEPPER_B_ENABLE_PIN = 9;
constexpr bool STEPPER_INPUT_ACTIVE_HIGH = true;
constexpr uint32_t MOTOR_FULL_STEPS_PER_REV = 200;
constexpr uint32_t MOTOR_MICROSTEPS = 16;
constexpr uint32_t DEFAULT_STEPPER_SPEED_SPS = 3200;
constexpr uint32_t ACTUATOR_TICK_HZ = 25000;
constexpr uint32_t ACTUATOR_TICK_US = 40;
constexpr uint32_t MAX_STEPPER_SPEED_SPS = ACTUATOR_TICK_HZ / 2;

constexpr uint8_t SERVO_A_PIN = 10;
constexpr uint8_t SERVO_B_PIN = 11;
constexpr uint8_t EVENT_TTL_OUT_PIN = 12;
constexpr uint32_t EVENT_PULSE_WIDTH_US = 5000;

constexpr uint8_t SERVO_A_HOME_DEG = 90;
constexpr uint8_t SERVO_B_HOME_DEG = 90;
constexpr uint16_t SERVO_MIN_PULSE_US = 1000;
constexpr uint16_t SERVO_MAX_PULSE_US = 2000;
constexpr uint32_t SERVO_FRAME_US = 20000;

constexpr uint8_t STATUS_LED_PIN = 13;
constexpr uint8_t BRUSH_CONTACT_IN_PIN = A0;
constexpr uint8_t LICK_SENSOR_IN_PIN = A1;
constexpr uint8_t CAMERA_TRIGGER_OUT_PIN = A2;
constexpr uint8_t REWARD_VALVE_OUT_PIN = A3;  // Reserved; output remains LOW.

constexpr bool SENSOR_ACTIVE_HIGH = true;
constexpr uint32_t SENSOR_DEBOUNCE_US = 1000;
constexpr uint32_t CAMERA_TRIGGER_WIDTH_US = 1000;

// ATmega328P has only 2 KB SRAM. Capacity is 32 (31 usable) by design.
constexpr uint8_t EVENT_QUEUE_CAPACITY = 32;
constexpr uint8_t MAX_LOG_RECORDS_PER_LOOP = 2;
constexpr uint8_t MAX_SERIAL_BYTES_PER_LOOP = 32;
constexpr uint8_t SERIAL_COMMAND_BUFFER_SIZE = 96;

}  // namespace Config
