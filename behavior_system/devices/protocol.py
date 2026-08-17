from __future__ import annotations

from dataclasses import dataclass, field

from behavior_system.core.events import Event


UINT32_MODULUS = 1 << 32
UINT32_HALF_RANGE = 1 << 31


@dataclass(slots=True)
class MasterTimestampUnwrapper:
    """Extend ordered uint32_t micros() samples across rollover."""

    epoch_us: int = 0
    last_raw_us: int | None = None

    def unwrap(self, raw_us: int) -> int:
        if not 0 <= raw_us < UINT32_MODULUS:
            raise ValueError("master timestamp must be an unsigned 32-bit value")
        if self.last_raw_us is None:
            self.last_raw_us = raw_us
            return raw_us
        if raw_us < self.last_raw_us:
            if self.last_raw_us - raw_us > UINT32_HALF_RANGE:
                self.epoch_us += UINT32_MODULUS
            else:
                # A slightly late queued record must not move the rollover
                # reference backwards and corrupt the next sample.
                return self.epoch_us + raw_us
        self.last_raw_us = raw_us
        return self.epoch_us + raw_us


@dataclass(slots=True)
class DeviceProtocolDecoder:
    clock: MasterTimestampUnwrapper = field(default_factory=MasterTimestampUnwrapper)

    def decode(self, line: str) -> Event:
        parts = [part.strip() for part in line.strip().split(",")]
        kind = parts[0].upper() if parts and parts[0] else ""

        if kind == "EVENT" and len(parts) == 5:
            raw_us = _parse_uint32(parts[1], "event timestamp")
            trial = _parse_nonnegative(parts[2], "trial number")
            value = _parse_int(parts[4], "event value")
            return Event(
                source="arduino",
                event_type=parts[3],
                master_timestamp_us=self.clock.unwrap(raw_us),
                trial_number=trial,
                value=value,
                payload={"raw_master_timestamp_us": raw_us, "line": line},
            )

        if kind == "FRAME" and len(parts) == 3:
            frame_number = _parse_nonnegative(parts[1], "frame number")
            raw_us = _parse_uint32(parts[2], "frame timestamp")
            return Event(
                source="two_photon",
                event_type="FRAME",
                master_timestamp_us=self.clock.unwrap(raw_us),
                value=frame_number,
                payload={"frame_number": frame_number, "raw_master_timestamp_us": raw_us},
            )

        if kind in {"STATUS", "ACK", "ERR"}:
            return Event(
                source="device",
                event_type=kind,
                payload={"fields": parts[1:], "line": line},
            )

        return Event(source="device", event_type="LINE", payload={"line": line})


def _parse_uint32(text: str, label: str) -> int:
    value = _parse_nonnegative(text, label)
    if value >= UINT32_MODULUS:
        raise ValueError(f"{label} is outside uint32 range: {text}")
    return value


def _parse_nonnegative(text: str, label: str) -> int:
    value = _parse_int(text, label)
    if value < 0:
        raise ValueError(f"{label} must be nonnegative: {text}")
    return value


def _parse_int(text: str, label: str) -> int:
    try:
        return int(text)
    except ValueError as exc:
        raise ValueError(f"invalid {label}: {text}") from exc
