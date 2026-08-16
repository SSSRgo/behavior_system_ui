from __future__ import annotations

from PySide6.QtCore import QObject, QTimer

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.modules.motor import MotorModule


class MotorAngleReciprocationTask(QObject):
    source = "task.angle_reciprocation"

    def __init__(self, motor: MotorModule, bus: EventBus) -> None:
        super().__init__()
        self.motor = motor
        self.bus = bus
        self.angle_degrees = 60.0
        self.interval_ms = 1000
        self.cycle_count = 0
        self.direction_sign = 1.0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._run_step)

    @property
    def running(self) -> bool:
        return self.timer.isActive()

    def start(self, angle_degrees: float = 60.0, interval_ms: int = 1000, immediate: bool = True) -> None:
        self.angle_degrees = max(0.001, abs(angle_degrees))
        self.interval_ms = max(1, interval_ms)
        self.cycle_count = 0
        self.direction_sign = 1.0
        self.timer.setInterval(self.interval_ms)
        self.timer.start()
        self._publish(
            "started",
            angle_degrees=self.angle_degrees,
            interval_ms=self.interval_ms,
            immediate=immediate,
        )

        if immediate:
            self._run_step()

    def start_from_config(self, parameters: dict) -> None:
        self.start(
            angle_degrees=float(parameters.get("angle_degrees", 60.0)),
            interval_ms=int(parameters.get("interval_ms", 1000)),
            immediate=bool(parameters.get("immediate", True)),
        )

    def stop(self) -> None:
        if not self.running:
            return

        self.timer.stop()
        self._publish("stopped", steps=self.cycle_count)

    def _run_step(self) -> None:
        self.cycle_count += 1
        degrees = self.angle_degrees * self.direction_sign
        direction = "forward" if degrees > 0 else "reverse"
        self._publish(
            "step_start",
            step=self.cycle_count,
            degrees=degrees,
            direction=direction,
            command=f"m{degrees / 360.0:g}",
        )
        self.motor.move_degrees(degrees)
        self.direction_sign *= -1.0

    def _publish(self, event_type: str, **payload) -> None:
        self.bus.publish(Event(source=self.source, event_type=event_type, payload=payload))
