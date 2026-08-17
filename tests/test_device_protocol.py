from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from behavior_system.core.events import Event
from behavior_system.core.session import EventLogger, SessionConfig
from behavior_system.devices.protocol import (
    DeviceProtocolDecoder,
    MasterTimestampUnwrapper,
    UINT32_MODULUS,
)


class MasterTimestampUnwrapperTests(unittest.TestCase):
    def test_extends_rollover(self) -> None:
        clock = MasterTimestampUnwrapper()
        self.assertEqual(clock.unwrap(UINT32_MODULUS - 3), UINT32_MODULUS - 3)
        self.assertEqual(clock.unwrap(5), UINT32_MODULUS + 5)

    def test_small_out_of_order_value_does_not_create_epoch(self) -> None:
        clock = MasterTimestampUnwrapper()
        self.assertEqual(clock.unwrap(100), 100)
        self.assertEqual(clock.unwrap(90), 90)
        self.assertEqual(clock.unwrap(110), 110)


class DeviceProtocolDecoderTests(unittest.TestCase):
    def test_decodes_event_with_master_time_and_trial(self) -> None:
        event = DeviceProtocolDecoder().decode("EVENT,183472913,23,BRUSH_COMMAND,120")
        self.assertEqual(event.source, "arduino")
        self.assertEqual(event.event_type, "BRUSH_COMMAND")
        self.assertEqual(event.master_timestamp_us, 183472913)
        self.assertEqual(event.trial_number, 23)
        self.assertEqual(event.value, 120)

    def test_decodes_frame(self) -> None:
        event = DeviceProtocolDecoder().decode("FRAME,16910,183450001")
        self.assertEqual(event.source, "two_photon")
        self.assertEqual(event.event_type, "FRAME")
        self.assertEqual(event.value, 16910)
        self.assertEqual(event.master_timestamp_us, 183450001)

    def test_rejects_malformed_numeric_field(self) -> None:
        with self.assertRaisesRegex(ValueError, "event timestamp"):
            DeviceProtocolDecoder().decode("EVENT,nope,23,BRUSH_COMMAND,120")

    def test_keeps_status_separate_from_timed_events(self) -> None:
        event = DeviceProtocolDecoder().decode("STATUS,ARMED,TRIAL,0")
        self.assertEqual(event.source, "device")
        self.assertEqual(event.event_type, "STATUS")
        self.assertIsNone(event.master_timestamp_us)


class SessionLoggerTests(unittest.TestCase):
    def test_writes_master_alignment_fields_to_csv(self) -> None:
        with TemporaryDirectory() as temp_dir:
            logger = EventLogger(Path(temp_dir))
            session_dir = logger.start(SessionConfig(task_name="test"))
            logger.log(
                Event(
                    source="arduino",
                    event_type="BRUSH_CONTACT",
                    master_timestamp_us=123456,
                    trial_number=7,
                    value=1,
                )
            )
            logger.stop()
            csv_text = (session_dir / "events.csv").read_text(encoding="utf-8")
            self.assertIn("master_timestamp_us", csv_text)
            self.assertIn("123456,7,arduino,BRUSH_CONTACT,1", csv_text)


if __name__ == "__main__":
    unittest.main()
