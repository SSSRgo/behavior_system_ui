from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QObject

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.modules.led import LEDModule
from behavior_system.modules.lick import LickModule
from behavior_system.modules.motor import MotorModule
from behavior_system.modules.water import WaterModule
from behavior_system.tasks.angle_reciprocation import MotorAngleReciprocationTask
from behavior_system.tasks.config import TaskConfig
from behavior_system.tasks.motor_every_second import MotorEverySecondTask
from behavior_system.tasks.script_task import ScriptTaskWrapper, TaskContext
from behavior_system.tasks.arduino_experiment import ArduinoExperimentTask


class TaskRegistry:
    def __init__(
        self,
        motor: MotorModule,
        bus: EventBus,
        lick: LickModule | None = None,
        water: WaterModule | None = None,
        led: LEDModule | None = None,
    ) -> None:
        self.motor = motor
        self.bus = bus
        self.lick = lick
        self.water = water
        self.led = led

    def create(self, config: TaskConfig) -> QObject:
        if config.task_type == "arduino_experiment":
            return ArduinoExperimentTask(self.motor.transport, self.bus)
        if config.task_type == "motor_every_second":
            return MotorEverySecondTask(self.motor, self.bus)
        if config.task_type == "angle_reciprocation":
            return MotorAngleReciprocationTask(self.motor, self.bus)
        if config.task_type == "script":
            raw_script_path = config.parameters.get("script_path")
            if not raw_script_path:
                raise ValueError("Script task is missing parameters.script_path")
            script_path = Path(raw_script_path)
            if not script_path.is_absolute() and config.path is not None:
                script_path = (config.path.parent / script_path).resolve()
            context = TaskContext(
                motor=self.motor,
                lick=self.lick,
                water=self.water,
                led=self.led,
                bus=self.bus,
            )
            return ScriptTaskWrapper(script_path, context)

        self.bus.publish(
            Event(
                source="task.registry",
                event_type="unknown_task_type",
                payload={"task_type": config.task_type, "name": config.name},
            )
        )
        raise ValueError(f"Unknown task_type: {config.task_type}")
