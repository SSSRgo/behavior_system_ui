from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class MotorModule(HardwareModule):
    info = ModuleInfo(
        name="motor",
        display_name="PD42S1 Motor",
        description="Arduino Uno STEP/DIR controller for PD42S1.",
    )

    def set_rpm(self, rpm: float) -> None:
        self.send(f"v{rpm:g}", event_type="set_rpm", rpm=rpm)

    def move_revolutions(self, revolutions: float) -> None:
        self.send(f"m{revolutions:g}", event_type="move_revolutions", revolutions=revolutions)

    def move_degrees(self, degrees: float) -> None:
        revolutions = degrees / 360.0
        self.send(f"m{revolutions:g}", event_type="move_degrees", degrees=degrees, revolutions=revolutions)

    def move_forward_one_rev(self) -> None:
        self.send("f", event_type="move_one_revolution", direction="forward")

    def move_reverse_one_rev(self) -> None:
        self.send("r", event_type="move_one_revolution", direction="reverse")

    def swing_120_once(self) -> None:
        self.send("o", event_type="swing_120_once")

    def start_swing_120(self) -> None:
        self.send("p", event_type="start_swing_120")

    def stop_swing_120(self) -> None:
        self.send("s", event_type="stop_swing_120")

    def toggle_enable(self) -> None:
        self.send("e", event_type="toggle_enable")

    def configure_driver(self) -> None:
        self.send("c", event_type="configure_driver")
