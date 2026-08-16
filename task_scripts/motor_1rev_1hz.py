from __future__ import annotations

from PySide6.QtCore import QObject, QTimer


class Task(QObject):
    source = "task.script.motor_1rev_1hz"

    def __init__(self, ctx) -> None:
        super().__init__()
        self.ctx = ctx
        self.cycle = 0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.run_cycle)

    def start(self, parameters: dict | None = None) -> None:
        parameters = parameters or {}
        interval_ms = int(parameters.get("interval_ms", 1000))
        immediate = bool(parameters.get("immediate", True))

        self.cycle = 0
        self.timer.start(interval_ms)
        self.ctx.publish(self.source, "started", interval_ms=interval_ms, immediate=immediate)

        if immediate:
            self.run_cycle()

    def stop(self) -> None:
        self.timer.stop()
        self.ctx.publish(self.source, "stopped", cycles=self.cycle)

    def run_cycle(self) -> None:
        self.cycle += 1
        self.ctx.publish(self.source, "cycle_start", cycle=self.cycle, command="f")
        self.ctx.motor.move_forward_one_rev()


def create_task(ctx):
    return Task(ctx)
