from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass(slots=True)
class TaskConfig:
    name: str
    task_type: str
    description: str = ""
    parameters: dict[str, Any] = field(default_factory=dict)
    path: Path | None = None

    @classmethod
    def from_file(cls, path: Path) -> "TaskConfig":
        if path.suffix.lower() == ".py":
            return cls(
                name=path.stem,
                task_type="script",
                description=f"Python task script: {path.name}",
                parameters={"script_path": str(path)},
                path=path,
            )

        data = json.loads(path.read_text(encoding="utf-8"))
        return cls(
            name=str(data.get("name") or path.stem),
            task_type=str(data["task_type"]),
            description=str(data.get("description", "")),
            parameters=dict(data.get("parameters", {})),
            path=path,
        )

    def preview_text(self) -> str:
        return json.dumps(self.as_dict(), indent=2, ensure_ascii=False)

    def as_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "task_type": self.task_type,
            "description": self.description,
            "parameters": self.parameters,
            "path": str(self.path) if self.path else "",
        }


def list_task_configs(config_dir: Path) -> list[Path]:
    if not config_dir.exists():
        return []
    return sorted([*config_dir.glob("*.json"), *config_dir.glob("*.py")])
