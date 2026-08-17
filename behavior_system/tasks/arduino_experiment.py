from __future__ import annotations

from PySide6.QtCore import QObject

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.devices.serial_transport import Transport


PARAMETER_COMMANDS = {
    "brush_delay_us": "BRUSH_DELAY_US",
    "brush_duration_us": "BRUSH_DURATION_US",
    "trial_duration_us": "TRIAL_DURATION_US",
    "iti_us": "ITI_US",
    "trial_count": "TRIAL_COUNT",
    "servo_move_us": "SERVO_MOVE_US",
    "servo_home_deg": "SERVO_A_HOME",
    "servo_stim_deg": "SERVO_A_STIM",
    "event_pulse_us": "EVENT_PULSE_US",
}


class ArduinoExperimentTask(QObject):
    """Send configuration once; all trial timing then runs on the Arduino."""

    source = "task.arduino_experiment"

    def __init__(self, transport: Transport, bus: EventBus) -> None:
        super().__init__()
        self.transport = transport
        self.bus = bus
        self.running = False

    def start_from_config(self, parameters: dict) -> None:
        unknown = sorted(set(parameters) - set(PARAMETER_COMMANDS))
        if unknown:
            raise ValueError(f"Unknown Arduino experiment parameters: {', '.join(unknown)}")

        try:
            for key, command_name in PARAMETER_COMMANDS.items():
                if key in parameters:
                    self.transport.send_line(f"SET {command_name} {int(parameters[key])}")
            self.transport.send_line("ARM")
            self.transport.send_line("START")
        except Exception:
            try:
                self.transport.send_line("STOP")
            except Exception:
                pass
            raise
        self.running = True
        self.bus.publish(
            Event(source=self.source, event_type="configuration_sent", payload=dict(parameters))
        )

    def stop(self) -> None:
        if self.running:
            try:
                self.transport.send_line("STOP")
            except Exception as exc:
                self.bus.publish(
                    Event(
                        source=self.source,
                        event_type="stop_failed",
                        payload={"error": str(exc)},
                    )
                )
            else:
                self.bus.publish(Event(source=self.source, event_type="stop_requested"))
            finally:
                self.running = False
