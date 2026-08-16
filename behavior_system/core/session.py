from __future__ import annotations

import csv
import json
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any

from behavior_system.core.events import Event


@dataclass(slots=True)
class SessionConfig:
    animal_id: str = ""
    experimenter: str = ""
    task_name: str = "motor_test"
    notes: str = ""
    parameters: dict[str, Any] = field(default_factory=dict)


class EventLogger:
    def __init__(self, log_root: Path) -> None:
        self.log_root = log_root
        self.session_dir: Path | None = None
        self.csv_file = None
        self.jsonl_file = None
        self.csv_writer: csv.DictWriter | None = None

    @property
    def is_open(self) -> bool:
        return self.session_dir is not None

    def start(self, config: SessionConfig) -> Path:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        animal = _safe_name(config.animal_id or "animal")
        task = _safe_name(config.task_name or "task")
        self.session_dir = self.log_root / f"{stamp}_{animal}_{task}"
        self.session_dir.mkdir(parents=True, exist_ok=False)

        (self.session_dir / "session_config.json").write_text(
            json.dumps(asdict(config), indent=2),
            encoding="utf-8",
        )

        self.csv_file = (self.session_dir / "events.csv").open("w", newline="", encoding="utf-8")
        self.jsonl_file = (self.session_dir / "events.jsonl").open("w", encoding="utf-8")
        self.csv_writer = csv.DictWriter(self.csv_file, fieldnames=["timestamp", "source", "event_type", "payload"])
        self.csv_writer.writeheader()
        return self.session_dir

    def log(self, event: Event) -> None:
        if not self.is_open or self.csv_writer is None or self.jsonl_file is None:
            return

        row = event.as_row()
        csv_row = dict(row)
        csv_row["payload"] = json.dumps(row["payload"], ensure_ascii=False)
        self.csv_writer.writerow(csv_row)
        self.csv_file.flush()

        self.jsonl_file.write(json.dumps(row, ensure_ascii=False) + "\n")
        self.jsonl_file.flush()

    def stop(self) -> None:
        if self.csv_file is not None:
            self.csv_file.close()
        if self.jsonl_file is not None:
            self.jsonl_file.close()

        self.session_dir = None
        self.csv_file = None
        self.jsonl_file = None
        self.csv_writer = None


def _safe_name(value: str) -> str:
    cleaned = "".join(ch if ch.isalnum() or ch in ("-", "_") else "_" for ch in value.strip())
    return cleaned or "session"
