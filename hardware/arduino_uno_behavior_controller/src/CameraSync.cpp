#include "CameraSync.h"

#include "MasterClock.h"

CameraSync *CameraSync::instance_ = nullptr;

void CameraSync::begin(uint8_t triggerOutPin, uint8_t frameInPin, uint32_t pulseWidthUs) {
  triggerOutPin_ = triggerOutPin;
  frameInPin_ = frameInPin;
  pulseWidthUs_ = constrain(pulseWidthUs, 10UL, 1000000UL);
  pinMode(triggerOutPin_, OUTPUT);
  digitalWrite(triggerOutPin_, LOW);
  pinMode(frameInPin_, INPUT);
  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(frameInPin_), frameISRThunk, RISING);
}

void CameraSync::singleTrigger() {
  startPulse(MasterClock::nowUs());
}

void CameraSync::startTriggerTrain(uint32_t periodUs) {
  trainPeriodUs_ = constrain(periodUs, pulseWidthUs_ + 100UL, 1800000000UL);
  nextTriggerUs_ = MasterClock::nowUs();
  trainActive_ = true;
}

void CameraSync::stopTriggerTrain() {
  trainActive_ = false;
}

void CameraSync::update(uint32_t nowUs) {
  if (pulseActive_ && MasterClock::elapsed(nowUs, pulseStartedUs_, pulseWidthUs_)) {
    digitalWrite(triggerOutPin_, LOW);
    pulseActive_ = false;
  }

  if (!trainActive_ || pulseActive_ || !MasterClock::deadlineReached(nowUs, nextTriggerUs_)) {
    return;
  }
  startPulse(nowUs);
  const uint32_t periodsElapsed =
      static_cast<uint32_t>(nowUs - nextTriggerUs_) / trainPeriodUs_ + 1U;
  nextTriggerUs_ += periodsElapsed * trainPeriodUs_;
}

void CameraSync::startPulse(uint32_t timestampUs) {
  digitalWrite(triggerOutPin_, HIGH);
  pulseStartedUs_ = timestampUs;
  pulseActive_ = true;
  logger_.logAt(timestampUs, EventID::CAMERA_TRIGGER, 0, 1);
}

void CameraSync::frameISRThunk() {
  if (instance_ != nullptr) {
    instance_->handleFrameISR(MasterClock::nowUs());
  }
}

void CameraSync::handleFrameISR(uint32_t timestampUs) {
  const uint32_t frame = ++frameNumber_;
  logger_.logFromISR(timestampUs, EventID::CAMERA_FRAME, 0, static_cast<int32_t>(frame));
}
