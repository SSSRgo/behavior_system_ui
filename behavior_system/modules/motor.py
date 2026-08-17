from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class MotorModule(HardwareModule):
    info = ModuleInfo(
        name="motor",
        display_name="PD42S1 Motor",
        description="Arduino Uno non-blocking STEP/DIR controller for PD42S1.",
    )

    def set_rpm(self, rpm: float) -> None:
        steps_per_second = round(rpm * 200 * 16 / 60)
        self.send(
            f"SET STEPPER_A_SPEED_SPS {steps_per_second}",
            event_type="set_rpm",
            rpm=rpm,
            steps_per_second=steps_per_second,
        )

    def move_revolutions(self, revolutions: float) -> None:
        self.send(
            f"MOVE STEPPER_A_REV {revolutions:g}",
            event_type="move_revolutions",
            revolutions=revolutions,
        )

    def move_degrees(self, degrees: float) -> None:
        steps = round(degrees / 360.0 * 200 * 16)
        self.send(
            f"MOVE STEPPER_A {steps}",
            event_type="move_degrees",
            degrees=degrees,
            steps=steps,
        )

    def move_forward_one_rev(self) -> None:
        self.send("MOVE STEPPER_A 3200", event_type="move_one_revolution", direction="forward")

    def move_reverse_one_rev(self) -> None:
        self.send("MOVE STEPPER_A -3200", event_type="move_one_revolution", direction="reverse")

    def swing_120_once(self) -> None:
        self.send("SWING ONCE", event_type="swing_120_once")

    def start_swing_120(self) -> None:
        self.send("SWING START", event_type="start_swing_120")

    def stop_swing_120(self) -> None:
        self.send("SWING STOP", event_type="stop_swing_120")

    def toggle_enable(self) -> None:
        self.send("ENABLE STEPPER_A TOGGLE", event_type="toggle_enable")

    def configure_driver(self) -> None:
        self.send("c", event_type="configure_driver")
