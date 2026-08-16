from __future__ import annotations

from PySide6.QtCore import QObject, QTimer


class Task(QObject):
    source = "task.script.burst_60deg_n_delay_repeat"

    def __init__(self, ctx) -> None:
        super().__init__()
        self.ctx = ctx
        self.timer = QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.timeout.connect(self.run_next_step)

        self.angle_degrees = 60.0
        self.n = 5
        self.fast_interval_ms = 3000
        self.delay_ms = 10000
        self.block_index = 1
        self.burst_count = 0
        self.step_index = 0
        self.direction_sign = 1.0
        self.running = False

    def start(self, parameters: dict | None = None) -> None:
        parameters = parameters or {}
        self.angle_degrees = float(parameters.get("angle_degrees", 60.0))
        self.n = max(1, int(parameters.get("n", 5)))
        self.fast_interval_ms = max(1, int(parameters.get("fast_interval_ms", 3000)))
        self.delay_ms = max(0, int(parameters.get("delay_ms", 5000)))

        self.block_index = 1
        self.burst_count = 0
        self.step_index = 0
        self.direction_sign = 1.0
        self.running = True

        self.ctx.publish(
            self.source,
            "started",
            angle_degrees=self.angle_degrees,
            n=self.n,
            fast_interval_ms=self.fast_interval_ms,
            delay_ms=self.delay_ms,
        )
        self.run_next_step()

    def stop(self) -> None:
        self.running = False
        self.timer.stop()
        self.ctx.publish(
            self.source,
            "stopped",
            block=self.block_index,
            step=self.step_index,
        )

    def run_next_step(self) -> None:
        if not self.running:
            return

        if self.step_index >= self.n * 2:
            self.burst_count += 1
            self.ctx.publish(
                self.source,
                "burst_completed",
                burst=self.burst_count,
                block=self.block_index,
                n=self.n,
            )
            self.block_index += 1
            self.step_index = 0
            self.direction_sign = 1.0
            self.ctx.publish(
                self.source,
                "delay_start",
                burst=self.burst_count,
                delay_ms=self.delay_ms,
            )
            self.timer.start(self.delay_ms)
            return

        degrees = self.angle_degrees * self.direction_sign
        self.step_index += 1
        self.ctx.publish(
            self.source,
            "move",
            block=self.block_index,
            step=self.step_index,
            degrees=degrees,
            command=f"m{degrees / 360.0:g}",
        )
        self.ctx.motor.move_degrees(degrees)
        self.direction_sign *= -1.0
        self.timer.start(self.fast_interval_ms)


def create_task(ctx):
    return Task(ctx)
