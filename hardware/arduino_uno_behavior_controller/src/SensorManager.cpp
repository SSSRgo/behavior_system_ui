#include "SensorManager.h"

#include <avr/interrupt.h>

#include "MasterClock.h"

SensorManager *SensorManager::instance_ = nullptr;

void SensorManager::begin(
    uint8_t brushContactPin,
    uint8_t lickPin,
    bool activeHigh,
    uint32_t debounceUs) {
  activeHigh_ = activeHigh;
  debounceUs_ = debounceUs;
  channels_[0].pin = brushContactPin;
  channels_[0].onEvent = EventID::BRUSH_CONTACT;
  channels_[0].offEvent = EventID::SENSOR_OFF;
  channels_[1].pin = lickPin;
  channels_[1].onEvent = EventID::LICK_ON;
  channels_[1].offEvent = EventID::LICK_OFF;

  // ATmega328P has no internal pulldown. Active-high inputs need an external
  // pulldown resistor; active-low inputs can use the internal pullup.
  const uint8_t inputMode = activeHigh_ ? INPUT : INPUT_PULLUP;
  pinMode(brushContactPin, inputMode);
  pinMode(lickPin, inputMode);
  for (uint8_t i = 0; i < 2; ++i) {
    channels_[i].inputRegister = portInputRegister(digitalPinToPort(channels_[i].pin));
    channels_[i].mask = digitalPinToBitMask(channels_[i].pin);
    channels_[i].lastLevel = (*channels_[i].inputRegister & channels_[i].mask) != 0;
  }

  instance_ = this;
  noInterrupts();
  // Config pins A0/A1 are PCINT8/PCINT9 on the ATmega328P (PCINT1 vector).
  PCICR |= _BV(PCIE1);
  PCMSK1 |= _BV(PCINT8) | _BV(PCINT9);
  interrupts();
}

void SensorManager::update(uint32_t nowUs) {
  (void)nowUs;
  // Edges are captured in the ISR; processing and serial output happen elsewhere.
}

void SensorManager::handlePinChangeInterrupt() {
  if (instance_ != nullptr) {
    instance_->handlePinChangesISR(MasterClock::nowUs());
  }
}

void SensorManager::handlePinChangesISR(uint32_t timestampUs) {
  for (uint8_t i = 0; i < 2; ++i) {
    const bool level = (*channels_[i].inputRegister & channels_[i].mask) != 0;
    if (level != channels_[i].lastLevel) {
      channels_[i].lastLevel = level;
      handleEdgeISR(i, timestampUs);
    }
  }
}

void SensorManager::handleEdgeISR(uint8_t channelIndex, uint32_t timestampUs) {
  Channel &channel = channels_[channelIndex];
  if (static_cast<uint32_t>(timestampUs - channel.lastEdgeUs) < debounceUs_) {
    return;
  }
  channel.lastEdgeUs = timestampUs;

  const bool level = (*channel.inputRegister & channel.mask) != 0;
  const bool active = level == activeHigh_;
  const EventID eventId = active ? channel.onEvent : channel.offEvent;
  logger_.logFromISR(timestampUs, eventId, trialNumber_, active ? 1 : 0);
  if (active && eventId == EventID::BRUSH_CONTACT) {
    sync_.triggerFromISR(timestampUs, eventId, trialNumber_);
  }
}

ISR(PCINT1_vect) {
  SensorManager::handlePinChangeInterrupt();
}
