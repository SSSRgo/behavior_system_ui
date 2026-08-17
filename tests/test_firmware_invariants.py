from __future__ import annotations

import re
import unittest
from pathlib import Path


FIRMWARE_SRC = (
    Path(__file__).resolve().parents[1]
    / "hardware"
    / "arduino_uno_behavior_controller"
    / "src"
)


class FirmwareInvariantTests(unittest.TestCase):
    def test_production_firmware_has_no_delay_calls(self) -> None:
        offenders: list[str] = []
        pattern = re.compile(r"\b(?:delay|delayMicroseconds)\s*\(")
        source_paths = [*FIRMWARE_SRC.glob("*.cpp"), *FIRMWARE_SRC.glob("*.h")]
        for path in sorted(source_paths):
            if pattern.search(path.read_text(encoding="utf-8")):
                offenders.append(path.name)
        self.assertEqual(offenders, [])

    def test_production_firmware_has_no_dynamic_string(self) -> None:
        offenders: list[str] = []
        source_paths = [*FIRMWARE_SRC.glob("*.cpp"), *FIRMWARE_SRC.glob("*.h")]
        for path in sorted(source_paths):
            if re.search(r"\bString\b", path.read_text(encoding="utf-8")):
                offenders.append(path.name)
        self.assertEqual(offenders, [])

    def test_uno_scheduler_budget_is_explicit(self) -> None:
        config = (FIRMWARE_SRC / "config.h").read_text(encoding="utf-8")
        self.assertIn("ACTUATOR_TICK_HZ = 25000", config)
        self.assertIn("ACTUATOR_TICK_US = 40", config)
        self.assertIn("MAX_STEPPER_SPEED_SPS = ACTUATOR_TICK_HZ / 2", config)

    def test_rollover_safe_clock_helpers_are_used(self) -> None:
        clock = (FIRMWARE_SRC / "MasterClock.h").read_text(encoding="utf-8")
        self.assertIn("now - start", clock)
        self.assertIn("now - deadline", clock)

    def test_event_ids_are_unique(self) -> None:
        events = (FIRMWARE_SRC / "events.h").read_text(encoding="utf-8")
        values = [int(value) for value in re.findall(r"^\s*[A-Z0-9_]+\s*=\s*(\d+)", events, re.MULTILINE)]
        self.assertEqual(len(values), len(set(values)))


if __name__ == "__main__":
    unittest.main()
