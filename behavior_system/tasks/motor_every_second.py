from __future__ import annotations

from PySide6.QtCore import QObject, QTimer

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.modules.motor import MotorModule


class MotorEverySecondTask(QObject):
    source = "task.motor_1hz"

    def __init__(self, motor: MotorModule, bus: EventBus) -> None:
        super().__init__()
        self.motor = motor
        self.bus = bus
        self.interval_ms = 1000
        self.cycle_count = 0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._run_cycle)

    @property
    def running(self) -> bool:
        return self.timer.isActive()

    def start(self, interval_ms: int = 1000, immediate: bool = True) -> None:
        self.interval_ms = max(1, interval_ms)
        self.cycle_count = 0
        self.timer.setInterval(self.interval_ms)
        self.timer.start()
        self._publish("started", interval_ms=self.interval_ms, immediate=immediate)

        if immediate:
            self._run_cycle()

    def start_from_config(self, parameters: dict) -> None:
        self.start(
            interval_ms=int(parameters.get("interval_ms", 1000)),
            immediate=bool(parameters.get("immediate", True)),
        )

    def stop(self) -> None:
        if not self.running:
            return

        self.timer.stop()
        self._publish("stopped", cycles=self.cycle_count)

    def _run_cycle(self) -> None:
        self.cycle_count += 1
        self._publish("cycle_start", cycle=self.cycle_count, command="f")
        self.motor.move_forward_one_rev()

    def _publish(self, event_type: str, **payload) -> None:
        self.bus.publish(Event(source=self.source, event_type=event_type, payload=payload))
