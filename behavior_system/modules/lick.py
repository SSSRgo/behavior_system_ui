from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class LickModule(HardwareModule):
    info = ModuleInfo(
        name="lick",
        display_name="Lick Sensor",
        description="Placeholder module for future lickometer input.",
    )

    def arm(self) -> None:
        self.publish("arm_requested")

    def disarm(self) -> None:
        self.publish("disarm_requested")

    def simulate_lick(self) -> None:
        self.publish("lick", simulated=True)
