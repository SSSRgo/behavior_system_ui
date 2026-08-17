from __future__ import annotations

import sys
import unittest
from types import ModuleType


try:
    from PySide6.QtCore import QObject as _QObject
except ModuleNotFoundError:
    qt_core = ModuleType("PySide6.QtCore")
    qt_core.QObject = object

    class Signal:
        def __init__(self, *_args) -> None:
            return

        def emit(self, _value) -> None:
            return

    qt_core.Signal = Signal
    py_side = ModuleType("PySide6")
    py_side.QtCore = qt_core
    sys.modules["PySide6"] = py_side
    sys.modules["PySide6.QtCore"] = qt_core

from behavior_system.modules.servo import ServoModule


class RecordingTransport:
    def __init__(self) -> None:
        self.lines: list[str] = []

    @property
    def connected(self) -> bool:
        return True

    def send_line(self, line: str) -> None:
        self.lines.append(line)

    def close(self) -> None:
        return


class RecordingBus:
    def __init__(self) -> None:
        self.events = []

    def publish(self, event) -> None:
        self.events.append(event)


class ServoModuleTests(unittest.TestCase):
    def test_moves_each_channel_using_firmware_protocol(self) -> None:
        transport = RecordingTransport()
        module = ServoModule(transport, RecordingBus())

        module.move_a(45, 150)
        module.move_b(135, 200)

        self.assertEqual(
            transport.lines,
            ["MOVE SERVO_A 45 150000", "MOVE SERVO_B 135 200000"],
        )

    def test_move_and_center_both(self) -> None:
        transport = RecordingTransport()
        module = ServoModule(transport, RecordingBus())

        module.move_both(60, 120, 100)
        module.center_both(50)

        self.assertEqual(
            transport.lines,
            [
                "MOVE SERVO_A 60 100000",
                "MOVE SERVO_B 120 100000",
                "MOVE SERVO_A 90 50000",
                "MOVE SERVO_B 90 50000",
            ],
        )

    def test_rejects_invalid_angle(self) -> None:
        module = ServoModule(RecordingTransport(), RecordingBus())

        with self.assertRaisesRegex(ValueError, "between 0 and 180"):
            module.move_a(181)


if __name__ == "__main__":
    unittest.main()
