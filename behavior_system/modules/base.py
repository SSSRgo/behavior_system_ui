from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.devices.serial_transport import Transport


@dataclass(slots=True)
class ModuleInfo:
    name: str
    display_name: str
    description: str = ""


class HardwareModule:
    info = ModuleInfo(name="base", display_name="Base")

    def __init__(self, transport: Transport, bus: EventBus) -> None:
        self.transport = transport
        self.bus = bus
        self.enabled = True

    def set_transport(self, transport: Transport) -> None:
        self.transport = transport

    def publish(self, event_type: str, **payload: Any) -> None:
        self.bus.publish(Event(source=self.info.name, event_type=event_type, payload=payload))

    def send(self, command: str, event_type: str = "command", **payload: Any) -> None:
        if not self.enabled:
            self.publish("blocked", reason="module disabled", command=command)
            return

        try:
            self.transport.send_line(command)
        except Exception as exc:
            self.publish("command_failed", command=command, error=str(exc), **payload)
            return

        self.publish(event_type, command=command, **payload)
