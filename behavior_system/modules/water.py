from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class WaterModule(HardwareModule):
    info = ModuleInfo(
        name="water",
        display_name="Water Reward",
        description="Placeholder module for future solenoid valve reward control.",
    )

    def reward(self, duration_ms: int = 20) -> None:
        self.publish("reward_requested", duration_ms=duration_ms)
