from __future__ import annotations

import importlib.util
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType
from typing import Any

from PySide6.QtCore import QObject

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.modules.led import LEDModule
from behavior_system.modules.lick import LickModule
from behavior_system.modules.motor import MotorModule
from behavior_system.modules.water import WaterModule


@dataclass(slots=True)
class TaskContext:
    motor: MotorModule
    lick: LickModule | None
    water: WaterModule | None
    led: LEDModule | None
    bus: EventBus

    def publish(self, source: str, event_type: str, **payload: Any) -> None:
        self.bus.publish(Event(source=source, event_type=event_type, payload=payload))


class ScriptTaskWrapper(QObject):
    source = "task.script"

    def __init__(self, script_path: Path, context: TaskContext) -> None:
        super().__init__()
        self.script_path = script_path
        self.context = context
        self.module: ModuleType | None = None
        self.task = None

    def start_from_config(self, parameters: dict) -> None:
        self.module = _load_script_module(self.script_path)
        self.task = _create_script_task(self.module, self.context)

        if not hasattr(self.task, "start"):
            raise AttributeError(f"{self.script_path.name} task object must implement start(parameters)")

        self.context.publish(self.source, "script_loaded", path=str(self.script_path))
        self.task.start(parameters)
        self.context.publish(self.source, "started", path=str(self.script_path), parameters=parameters)

    def stop(self) -> None:
        if self.task is not None and hasattr(self.task, "stop"):
            self.task.stop()
        self.context.publish(self.source, "stopped", path=str(self.script_path))


def _load_script_module(path: Path) -> ModuleType:
    module_name = f"behavior_task_{path.stem}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load task script: {path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _create_script_task(module: ModuleType, context: TaskContext):
    if hasattr(module, "create_task"):
        return module.create_task(context)
    if hasattr(module, "Task"):
        return module.Task(context)
    raise AttributeError("Task script must define create_task(ctx) or Task(ctx)")
