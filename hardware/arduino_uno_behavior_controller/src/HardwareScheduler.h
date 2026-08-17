#pragma once

#include <Arduino.h>

#include "config.h"

class HardwareScheduler {
 public:
  void begin(
      uint8_t stepperAStepPin,
      uint8_t stepperBStepPin,
      uint8_t servoAPin,
      uint8_t servoBPin);

  bool startStepper(uint8_t index, uint32_t pulses, uint32_t speedStepsPerSecond, int8_t directionSign);
  void stopStepper(uint8_t index);
  bool stepperBusy(uint8_t index) const;
  bool takeStepperCompletion(uint8_t index);
  int32_t stepperPosition(uint8_t index) const;

  void enableServo(uint8_t index, bool enabled);
  void setServoPulseUs(uint8_t index, uint16_t pulseWidthUs);

  static void handleTimerInterrupt();

 private:
  struct StepperChannel {
    // Both values stay below ACTUATOR_TICK_HZ (25,000), so 16 bits are enough
    // and substantially shorten the 25 kHz AVR ISR.
    volatile uint16_t phase = 0;
    volatile uint16_t phaseIncrement = 0;
    volatile uint32_t pulsesRemaining = 0;
    volatile int32_t position = 0;
    volatile int8_t directionSign = 1;
    volatile bool stepHigh = false;
    volatile bool busy = false;
    volatile bool completionPending = false;
    volatile uint8_t *port = nullptr;
    uint8_t mask = 0;
  };

  struct ServoChannel {
    volatile uint8_t *port = nullptr;
    uint8_t mask = 0;
    volatile uint8_t pulseTicks = 75;
    volatile bool enabled = false;
  };

  static HardwareScheduler *instance_;
  void onTimerInterrupt();
  static void writeHigh(volatile uint8_t *port, uint8_t mask) { *port |= mask; }
  static void writeLow(volatile uint8_t *port, uint8_t mask) { *port &= static_cast<uint8_t>(~mask); }

  StepperChannel steppers_[2]{};
  ServoChannel servos_[2]{};
  volatile uint16_t servoFrameTick_ = 0;
};
