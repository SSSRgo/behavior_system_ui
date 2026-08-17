from __future__ import annotations

import unittest
import sys
from types import ModuleType


try:
    from PySide6.QtCore import QObject as _QObject
except ModuleNotFoundError:
    qt_core = ModuleType("PySide6.QtCore")
    qt_core.QObject = object
    qt_core.QTimer = type("QTimer", (), {})

    class Signal:
        def __init__(self, *_args) -> None:
            return

        def connect(self, _callback) -> None:
            return

        def emit(self, _value) -> None:
            return

    qt_core.Signal = Signal
    py_side = ModuleType("PySide6")
    py_side.QtCore = qt_core
    sys.modules["PySide6"] = py_side
    sys.modules["PySide6.QtCore"] = qt_core

from behavior_system.tasks.arduino_experiment import ArduinoExperimentTask


class RecordingTransport:
    def __init__(self, fail_on: str | None = None) -> None:
        self.lines: list[str] = []
        self.fail_on = fail_on

    @property
    def connected(self) -> bool:
        return True

    def send_line(self, line: str) -> None:
        self.lines.append(line)
        if line == self.fail_on:
            raise RuntimeError("injected transport failure")

    def close(self) -> None:
        return


class RecordingBus:
    def __init__(self) -> None:
        self.events = []

    def publish(self, event) -> None:
        self.events.append(event)


class ArduinoExperimentTaskTests(unittest.TestCase):
    def test_sends_configuration_before_arm_and_start(self) -> None:
        transport = RecordingTransport()
        task = ArduinoExperimentTask(transport, RecordingBus())

        task.start_from_config({"trial_count": 3, "brush_delay_us": 1000})

        self.assertEqual(
            transport.lines,
            [
                "SET BRUSH_DELAY_US 1000",
                "SET TRIAL_COUNT 3",
                "ARM",
                "START",
            ],
        )
        self.assertTrue(task.running)

        task.stop()
        self.assertEqual(transport.lines[-1], "STOP")
        self.assertFalse(task.running)

    def test_start_failure_attempts_safe_stop(self) -> None:
        transport = RecordingTransport(fail_on="START")
        task = ArduinoExperimentTask(transport, RecordingBus())

        with self.assertRaisesRegex(RuntimeError, "injected transport failure"):
            task.start_from_config({"trial_count": 3})

        self.assertEqual(transport.lines[-2:], ["START", "STOP"])
        self.assertFalse(task.running)

    def test_rejects_unknown_parameter_before_sending(self) -> None:
        transport = RecordingTransport()
        task = ArduinoExperimentTask(transport, RecordingBus())

        with self.assertRaisesRegex(ValueError, "Unknown Arduino experiment parameters"):
            task.start_from_config({"unknown": 1})

        self.assertEqual(transport.lines, [])


if __name__ == "__main__":
    unittest.main()
