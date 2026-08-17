#pragma once

#include <Arduino.h>

class MasterClock {
 public:
  static uint32_t nowUs() { return micros(); }

  static bool elapsed(uint32_t now, uint32_t start, uint32_t duration) {
    return static_cast<uint32_t>(now - start) >= duration;
  }

  static bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
  }
};

