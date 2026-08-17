#include "HardwareScheduler.h"

#include <avr/interrupt.h>

HardwareScheduler *HardwareScheduler::instance_ = nullptr;

void HardwareScheduler::begin(
    uint8_t stepperAStepPin,
    uint8_t stepperBStepPin,
    uint8_t servoAPin,
    uint8_t servoBPin) {
  const uint8_t stepPins[2] = {stepperAStepPin, stepperBStepPin};
  const uint8_t servoPins[2] = {servoAPin, servoBPin};

  for (uint8_t i = 0; i < 2; ++i) {
    pinMode(stepPins[i], OUTPUT);
    digitalWrite(stepPins[i], LOW);
    steppers_[i].port = portOutputRegister(digitalPinToPort(stepPins[i]));
    steppers_[i].mask = digitalPinToBitMask(stepPins[i]);

    pinMode(servoPins[i], OUTPUT);
    digitalWrite(servoPins[i], LOW);
    servos_[i].port = portOutputRegister(digitalPinToPort(servoPins[i]));
    servos_[i].mask = digitalPinToBitMask(servoPins[i]);
  }

  instance_ = this;
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = static_cast<uint16_t>((F_CPU / 8UL / Config::ACTUATOR_TICK_HZ) - 1UL);
  TCCR1B = _BV(WGM12) | _BV(CS11);  // CTC, prescaler 8.
  TIMSK1 = _BV(OCIE1A);
  interrupts();
}

bool HardwareScheduler::startStepper(
    uint8_t index,
    uint32_t pulses,
    uint32_t speedStepsPerSecond,
    int8_t directionSign) {
  if (index >= 2 || pulses == 0 || speedStepsPerSecond == 0 ||
      speedStepsPerSecond > Config::MAX_STEPPER_SPEED_SPS) {
    return false;
  }
  noInterrupts();
  StepperChannel &channel = steppers_[index];
  if (channel.busy) {
    interrupts();
    return false;
  }
  channel.phase = 0;
  channel.phaseIncrement = static_cast<uint16_t>(speedStepsPerSecond);
  channel.pulsesRemaining = pulses;
  channel.directionSign = directionSign;
  channel.stepHigh = false;
  channel.completionPending = false;
  channel.busy = true;
  writeLow(channel.port, channel.mask);
  interrupts();
  return true;
}

void HardwareScheduler::stopStepper(uint8_t index) {
  if (index >= 2) {
    return;
  }
  noInterrupts();
  StepperChannel &channel = steppers_[index];
  channel.busy = false;
  channel.stepHigh = false;
  channel.pulsesRemaining = 0;
  channel.completionPending = false;
  writeLow(channel.port, channel.mask);
  interrupts();
}

bool HardwareScheduler::stepperBusy(uint8_t index) const {
  return index < 2 && steppers_[index].busy;
}

bool HardwareScheduler::takeStepperCompletion(uint8_t index) {
  if (index >= 2 || !steppers_[index].completionPending) {
    return false;
  }
  noInterrupts();
  const bool pending = steppers_[index].completionPending;
  steppers_[index].completionPending = false;
  interrupts();
  return pending;
}

int32_t HardwareScheduler::stepperPosition(uint8_t index) const {
  if (index >= 2) {
    return 0;
  }
  noInterrupts();
  const int32_t position = steppers_[index].position;
  interrupts();
  return position;
}

void HardwareScheduler::enableServo(uint8_t index, bool enabled) {
  if (index >= 2) {
    return;
  }
  noInterrupts();
  servos_[index].enabled = enabled;
  if (!enabled) {
    writeLow(servos_[index].port, servos_[index].mask);
  }
  interrupts();
}

void HardwareScheduler::setServoPulseUs(uint8_t index, uint16_t pulseWidthUs) {
  if (index >= 2) {
    return;
  }
  pulseWidthUs = constrain(
      pulseWidthUs, Config::SERVO_MIN_PULSE_US, Config::SERVO_MAX_PULSE_US);
  const uint8_t ticks = static_cast<uint8_t>(
      (pulseWidthUs + Config::ACTUATOR_TICK_US / 2) / Config::ACTUATOR_TICK_US);
  servos_[index].pulseTicks = ticks;
}

void HardwareScheduler::handleTimerInterrupt() {
  if (instance_ != nullptr) {
    instance_->onTimerInterrupt();
  }
}

void HardwareScheduler::onTimerInterrupt() {
  for (uint8_t i = 0; i < 2; ++i) {
    StepperChannel &channel = steppers_[i];
    if (!channel.busy) {
      continue;
    }
    // Advance phase on every scheduler tick, including the high-pulse tick.
    // This preserves the requested average frequency while still guaranteeing
    // one full 40 us high tick and one full 40 us low tick at the 12.5 kstep/s cap.
    channel.phase += channel.phaseIncrement;
    if (channel.stepHigh) {
      writeLow(channel.port, channel.mask);
      channel.stepHigh = false;
      channel.position += channel.directionSign;
      if (--channel.pulsesRemaining == 0) {
        channel.busy = false;
        channel.completionPending = true;
      }
      continue;
    }

    if (channel.phase >= Config::ACTUATOR_TICK_HZ) {
      channel.phase -= Config::ACTUATOR_TICK_HZ;
      writeHigh(channel.port, channel.mask);
      channel.stepHigh = true;
    }
  }

  if (servoFrameTick_ == 0) {
    for (uint8_t i = 0; i < 2; ++i) {
      if (servos_[i].enabled) {
        writeHigh(servos_[i].port, servos_[i].mask);
      }
    }
  }
  for (uint8_t i = 0; i < 2; ++i) {
    if (servos_[i].enabled && servoFrameTick_ == servos_[i].pulseTicks) {
      writeLow(servos_[i].port, servos_[i].mask);
    }
  }

  ++servoFrameTick_;
  if (servoFrameTick_ >= Config::SERVO_FRAME_US / Config::ACTUATOR_TICK_US) {
    servoFrameTick_ = 0;
  }
}

ISR(TIMER1_COMPA_vect) {
  HardwareScheduler::handleTimerInterrupt();
}
