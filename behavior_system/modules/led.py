from __future__ import annotations

from behavior_system.modules.base import HardwareModule, ModuleInfo


class LEDModule(HardwareModule):
    info = ModuleInfo(
        name="led",
        display_name="LED",
        description="Placeholder module for future LED pulse control.",
    )

    def pulse(self, duration_ms: int = 100) -> None:
        self.publish("pulse_requested", duration_ms=duration_ms)
