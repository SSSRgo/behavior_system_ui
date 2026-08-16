from __future__ import annotations

import threading
import time
from collections.abc import Callable
from typing import Protocol


LineCallback = Callable[[str], None]


class Transport(Protocol):
    @property
    def connected(self) -> bool:
        ...

    def send_line(self, line: str) -> None:
        ...

    def close(self) -> None:
        ...


class NullTransport:
    @property
    def connected(self) -> bool:
        return False

    def send_line(self, line: str) -> None:
        raise RuntimeError("No device connected")

    def close(self) -> None:
        return


class MockTransport:
    def __init__(self, on_line: LineCallback | None = None) -> None:
        self._on_line = on_line
        self._connected = True

    @property
    def connected(self) -> bool:
        return self._connected

    def send_line(self, line: str) -> None:
        if self._on_line is not None:
            self._on_line(f"[mock] > {line.strip()}")

    def close(self) -> None:
        self._connected = False


class SerialTransport:
    def __init__(self, on_line: LineCallback | None = None) -> None:
        self._on_line = on_line
        self._serial = None
        self._reader_thread: threading.Thread | None = None
        self._stop_event = threading.Event()

    @property
    def connected(self) -> bool:
        return self._serial is not None and bool(self._serial.is_open)

    def open(self, port: str, baudrate: int = 115200, timeout: float = 0.1) -> None:
        import serial

        self.close()
        self._serial = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        self._stop_event.clear()
        self._reader_thread = threading.Thread(target=self._read_loop, name="serial-reader", daemon=True)
        self._reader_thread.start()

    def send_line(self, line: str) -> None:
        if not self.connected:
            raise RuntimeError("Serial port is not connected")

        data = line.strip().encode("ascii") + b"\n"
        self._serial.write(data)
        self._serial.flush()

    def close(self) -> None:
        self._stop_event.set()
        if self._reader_thread is not None and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=0.5)

        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None
        self._reader_thread = None

    def _read_loop(self) -> None:
        while not self._stop_event.is_set():
            if self._serial is None:
                return

            try:
                raw = self._serial.readline()
            except Exception as exc:
                if self._on_line is not None:
                    self._on_line(f"[serial-error] {exc}")
                return

            if raw:
                text = raw.decode("utf-8", errors="replace").strip()
                if text and self._on_line is not None:
                    self._on_line(text)
            else:
                time.sleep(0.01)


def list_serial_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except Exception:
        return []

    return [port.device for port in list_ports.comports()]
