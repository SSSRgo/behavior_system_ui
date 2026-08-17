from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class ServoModule(HardwareModule):
    info = ModuleInfo(
        name="servo",
        display_name="MG90S Servos",
        description="Two Arduino Uno servo channels on D10 and D11.",
    )

    def move_a(self, angle: int, duration_ms: int = 0) -> None:
        self._move("A", angle, duration_ms)

    def move_b(self, angle: int, duration_ms: int = 0) -> None:
        self._move("B", angle, duration_ms)

    def move_both(self, angle_a: int, angle_b: int, duration_ms: int = 0) -> None:
        self.move_a(angle_a, duration_ms)
        self.move_b(angle_b, duration_ms)

    def center_a(self, duration_ms: int = 0) -> None:
        self.move_a(90, duration_ms)

    def center_b(self, duration_ms: int = 0) -> None:
        self.move_b(90, duration_ms)

    def center_both(self, duration_ms: int = 0) -> None:
        self.move_both(90, 90, duration_ms)

    def _move(self, channel: str, angle: int, duration_ms: int) -> None:
        angle = int(angle)
        duration_ms = int(duration_ms)
        if not 0 <= angle <= 180:
            raise ValueError("servo angle must be between 0 and 180 degrees")
        if duration_ms < 0:
            raise ValueError("servo duration must be nonnegative")

        duration_us = duration_ms * 1000
        self.send(
            f"MOVE SERVO_{channel} {angle} {duration_us}",
            event_type="move",
            channel=channel,
            angle=angle,
            duration_ms=duration_ms,
        )
