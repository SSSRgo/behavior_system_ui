from __future__ import annotations

from PySide6.QtCore import QObject, QTimer


class Task(QObject):
    source = "task.script.lick_reward_template"

    def __init__(self, ctx) -> None:
        super().__init__()
        self.ctx = ctx
        self.reward_count = 0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.simulate_trial)

    def start(self, parameters: dict | None = None) -> None:
        parameters = parameters or {}
        self.interval_ms = int(parameters.get("interval_ms", 3000))
        self.reward_ms = int(parameters.get("reward_ms", 20))
        self.reward_count = 0
        self.timer.start(self.interval_ms)
        self.ctx.publish(self.source, "started", interval_ms=self.interval_ms, reward_ms=self.reward_ms)

    def stop(self) -> None:
        self.timer.stop()
        self.ctx.publish(self.source, "stopped", rewards=self.reward_count)

    def simulate_trial(self) -> None:
        # Replace this with real lick-event driven logic when the lick module is wired.
        self.reward_count += 1
        self.ctx.publish(self.source, "trial", reward_count=self.reward_count)
        if self.ctx.water is not None:
            self.ctx.water.reward(self.reward_ms)


def create_task(ctx):
    return Task(ctx)
