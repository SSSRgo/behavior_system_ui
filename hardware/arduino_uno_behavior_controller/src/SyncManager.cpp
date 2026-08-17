#include "SyncManager.h"

#include "MasterClock.h"

SyncManager *SyncManager::instance_ = nullptr;

void SyncManager::begin(uint8_t eventOutPin, uint8_t frameInPin, uint32_t pulseWidthUs) {
  eventOutPin_ = eventOutPin;
  frameInPin_ = frameInPin;
  setPulseWidthUs(pulseWidthUs);

  pinMode(eventOutPin_, OUTPUT);
  digitalWrite(eventOutPin_, LOW);
  eventOutPort_ = portOutputRegister(digitalPinToPort(eventOutPin_));
  eventOutMask_ = digitalPinToBitMask(eventOutPin_);
  pinMode(frameInPin_, INPUT);

  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(frameInPin_), frameISRThunk, RISING);
}

void SyncManager::setPulseWidthUs(uint32_t widthUs) {
  pulseWidthUs_ = constrain(widthUs, 10UL, 1000000UL);
}

void SyncManager::trigger(EventID sourceEvent, uint16_t trialNumber) {
  const uint32_t nowUs = MasterClock::nowUs();
  noInterrupts();
  startPulseUnsafe(nowUs);
  interrupts();
  logger_.logAt(
      nowUs, EventID::SYNC_OUT, trialNumber, static_cast<int32_t>(sourceEvent));
}

void SyncManager::triggerFromISR(
    uint32_t timestampUs, EventID sourceEvent, uint16_t trialNumber) {
  startPulseUnsafe(timestampUs);
  logger_.logFromISR(
      timestampUs, EventID::SYNC_OUT, trialNumber, static_cast<int32_t>(sourceEvent));
}

void SyncManager::startPulseUnsafe(uint32_t timestampUs) {
  // If events occur less than pulseWidthUs_ apart, the HIGH is extended. The event
  // queue still retains both source events, but one wire cannot encode two edges
  // while already HIGH; choose pulse widths/critical events accordingly.
  *eventOutPort_ |= eventOutMask_;
  pulseStartedUs_ = timestampUs;
  pulseActive_ = true;
}

void SyncManager::update(uint32_t nowUs) {
  noInterrupts();
  if (pulseActive_ && MasterClock::elapsed(nowUs, pulseStartedUs_, pulseWidthUs_)) {
    *eventOutPort_ &= static_cast<uint8_t>(~eventOutMask_);
    pulseActive_ = false;
  }
  interrupts();

  if (testActive_ && !pulseActive_ && MasterClock::deadlineReached(nowUs, testNextUs_)) {
    const int32_t jitterUs = static_cast<int32_t>(nowUs - testNextUs_);
    ++testPulseCount_;
    logger_.logAt(nowUs, EventID::TEST_SYNC_PULSE, testPulseCount_, jitterUs);
    trigger(EventID::TEST_SYNC_PULSE, testPulseCount_);
    const uint32_t periodsElapsed =
        static_cast<uint32_t>(nowUs - testNextUs_) / testIntervalUs_ + 1U;
    testNextUs_ += periodsElapsed * testIntervalUs_;

    if (testPulseCount_ >= testRequestedCount_) {
      testActive_ = false;
      logger_.log(EventID::TEST_SYNC_END, 0, static_cast<int32_t>(testPulseCount_));
    }
  }

  // Bounded burst mode can deliberately outrun serial flushing to exercise the
  // event queue. It is a software/logging test, not a substitute for TTL loopback.
  for (uint8_t i = 0;
       i < 32 && frameSimulationActive_ &&
       MasterClock::deadlineReached(nowUs, simulatedFrameNextUs_);
       ++i) {
    handleFrameISR(nowUs);
    ++simulatedFrameGeneratedCount_;
    simulatedFrameNextUs_ += simulatedFrameIntervalUs_;
    if (simulatedFrameGeneratedCount_ >= simulatedFrameRequestedCount_) {
      frameSimulationActive_ = false;
    }
  }
}

void SyncManager::startTest(uint32_t pulseCount, uint32_t intervalUs) {
  testRequestedCount_ = static_cast<uint16_t>(constrain(pulseCount, 1UL, 65535UL));
  testIntervalUs_ = constrain(intervalUs, pulseWidthUs_ + 100UL, 1800000000UL);
  testPulseCount_ = 0;
  testNextUs_ = MasterClock::nowUs();
  testActive_ = true;
  logger_.logAt(
      testNextUs_, EventID::TEST_SYNC_START, testRequestedCount_, static_cast<int32_t>(testIntervalUs_));
}

void SyncManager::stopTest() {
  if (!testActive_) {
    return;
  }
  testActive_ = false;
  logger_.log(EventID::TEST_SYNC_END, 0, static_cast<int32_t>(testPulseCount_));
}

void SyncManager::simulateFrameEdge() {
  handleFrameISR(MasterClock::nowUs());
}

void SyncManager::startFrameSimulation(uint32_t frameCount, uint32_t intervalUs) {
  simulatedFrameRequestedCount_ = constrain(frameCount, 1UL, 1000000UL);
  simulatedFrameIntervalUs_ = constrain(intervalUs, 1UL, 1800000000UL);
  simulatedFrameGeneratedCount_ = 0;
  simulatedFrameNextUs_ = MasterClock::nowUs();
  frameSimulationActive_ = true;
}

void SyncManager::stopFrameSimulation() {
  frameSimulationActive_ = false;
}

void SyncManager::frameISRThunk() {
  if (instance_ != nullptr) {
    instance_->handleFrameISR(MasterClock::nowUs());
  }
}

void SyncManager::handleFrameISR(uint32_t timestampUs) {
  const uint32_t frame = ++frameNumber_;
  logger_.logFromISR(
      timestampUs, EventID::TWO_PHOTON_FRAME, 0, static_cast<int32_t>(frame));
}
