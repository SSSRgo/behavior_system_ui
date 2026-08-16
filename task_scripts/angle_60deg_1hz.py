from __future__ import annotations

from PySide6.QtCore import QObject, QTimer


class Task(QObject):
    source = "task.script.angle_60deg_1hz"

    def __init__(self, ctx) -> None:
        super().__init__()
        self.ctx = ctx
        self.step = 0
        self.direction_sign = 1.0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.run_step)

    def start(self, parameters: dict | None = None) -> None:
        parameters = parameters or {}
        self.angle_degrees = float(parameters.get("angle_degrees", 60.0))
        interval_ms = int(parameters.get("interval_ms", 1000))
        immediate = bool(parameters.get("immediate", True))

        self.step = 0
        self.direction_sign = 1.0
        self.timer.start(interval_ms)
        self.ctx.publish(
            self.source,
            "started",
            angle_degrees=self.angle_degrees,
            interval_ms=interval_ms,
            immediate=immediate,
        )

        if immediate:
            self.run_step()

    def stop(self) -> None:
        self.timer.stop()
        self.ctx.publish(self.source, "stopped", steps=self.step)

    def run_step(self) -> None:
        self.step += 1
        degrees = self.angle_degrees * self.direction_sign
        self.ctx.publish(
            self.source,
            "step_start",
            step=self.step,
            degrees=degrees,
            command=f"m{degrees / 360.0:g}",
        )
        self.ctx.motor.move_degrees(degrees)
        self.direction_sign *= -1.0


def create_task(ctx):
    return Task(ctx)
