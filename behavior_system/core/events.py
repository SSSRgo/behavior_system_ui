from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


@dataclass(slots=True)
class Event:
    source: str
    event_type: str
    payload: dict[str, Any] = field(default_factory=dict)
    timestamp: str = field(default_factory=utc_now_iso)
    master_timestamp_us: int | None = None
    trial_number: int | None = None
    value: int | None = None

    def as_row(self) -> dict[str, Any]:
        return {
            "timestamp": self.timestamp,
            "master_timestamp_us": self.master_timestamp_us,
            "trial_number": self.trial_number,
            "source": self.source,
            "event_type": self.event_type,
            "value": self.value,
            "payload": self.payload,
        }
