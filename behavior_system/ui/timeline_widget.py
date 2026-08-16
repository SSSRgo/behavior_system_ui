from __future__ import annotations

import math
from datetime import datetime

from PySide6.QtCore import QRectF, QSize, Qt
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QSizePolicy, QWidget

from behavior_system.core.events import Event


class TimelineWidget(QWidget):
    def __init__(self, window_seconds: int = 30, max_events: int = 2000) -> None:
        super().__init__()
        self.window_seconds = window_seconds
        self.max_events = max_events
        self._origin_seconds: float | None = None
        self._events: list[tuple[float, Event]] = []
        self._lanes: list[str] = []
        self.setMinimumHeight(260)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def sizeHint(self) -> QSize:
        return QSize(620, 300)

    def set_window_seconds(self, value: int) -> None:
        self.window_seconds = max(1, value)
        self.update()

    def add_event(self, event: Event) -> None:
        event_seconds = _timestamp_to_seconds(event.timestamp)
        if self._origin_seconds is None:
            self._origin_seconds = event_seconds

        relative_seconds = event_seconds - self._origin_seconds
        if event.source not in self._lanes:
            self._lanes.append(event.source)

        self._events.append((relative_seconds, event))
        if len(self._events) > self.max_events:
            self._events = self._events[-self.max_events :]
        self.update()

    def clear(self) -> None:
        self._origin_seconds = None
        self._events.clear()
        self._lanes.clear()
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor("#fbfcfd"))

        if not self._events:
            painter.setPen(QColor("#6b7280"))
            painter.drawText(self.rect(), Qt.AlignCenter, "No timing events yet")
            return

        width = self.width()
        height = self.height()
        left = 112
        right = 18
        top = 26
        bottom = 30
        plot = QRectF(left, top, max(80, width - left - right), max(80, height - top - bottom))

        current_end = self._events[-1][0]
        current_start = max(0.0, current_end - float(self.window_seconds))
        visible_span = max(1.0, current_end - current_start)

        painter.setPen(QPen(QColor("#d1d5db"), 1))
        painter.drawRect(plot)
        self._draw_grid(painter, plot, current_start, current_end, visible_span)
        self._draw_lanes(painter, plot)
        self._draw_events(painter, plot, current_start, visible_span)

        painter.setPen(QColor("#374151"))
        painter.drawText(10, 17, f"Timing window: last {self.window_seconds}s")

    def _draw_grid(self, painter: QPainter, plot: QRectF, start: float, end: float, span: float) -> None:
        step = _nice_grid_step(span)
        first_tick = math.ceil(start / step) * step

        painter.setPen(QPen(QColor("#e5e7eb"), 1))
        tick = first_tick
        while tick <= end + 0.001:
            x = plot.left() + ((tick - start) / span) * plot.width()
            painter.drawLine(int(x), int(plot.top()), int(x), int(plot.bottom()))
            painter.setPen(QColor("#6b7280"))
            painter.drawText(int(x) - 18, int(plot.bottom()) + 18, f"{tick:.0f}s")
            painter.setPen(QPen(QColor("#e5e7eb"), 1))
            tick += step

    def _draw_lanes(self, painter: QPainter, plot: QRectF) -> None:
        lane_count = max(1, len(self._lanes))
        lane_height = plot.height() / lane_count

        for index, source in enumerate(self._lanes):
            y = plot.top() + lane_height * (index + 0.5)
            painter.setPen(QColor("#9ca3af"))
            painter.drawLine(int(plot.left()), int(y), int(plot.right()), int(y))
            painter.setPen(QColor("#111827"))
            painter.drawText(8, int(y) + 4, source[:15])

    def _draw_events(self, painter: QPainter, plot: QRectF, start: float, span: float) -> None:
        lane_count = max(1, len(self._lanes))
        lane_height = plot.height() / lane_count

        for seconds, event in self._events:
            if seconds < start:
                continue
            if seconds > start + span:
                continue

            lane_index = self._lanes.index(event.source)
            x = plot.left() + ((seconds - start) / span) * plot.width()
            y = plot.top() + lane_height * (lane_index + 0.5)

            color = _event_color(event.source, event.event_type)
            painter.setPen(QPen(color.darker(135), 1))
            painter.setBrush(color)
            painter.drawEllipse(QRectF(x - 4, y - 4, 8, 8))

            if event.event_type in ("started", "stopped", "cycle_start", "command_failed"):
                painter.setPen(color.darker(160))
                painter.drawText(int(x) + 5, int(y) - 7, event.event_type)


def _timestamp_to_seconds(timestamp: str) -> float:
    return datetime.fromisoformat(timestamp).timestamp()


def _nice_grid_step(span: float) -> int:
    if span <= 5:
        return 1
    if span <= 15:
        return 2
    if span <= 45:
        return 5
    if span <= 90:
        return 10
    if span <= 180:
        return 30
    return 60


def _event_color(source: str, event_type: str) -> QColor:
    if event_type == "command_failed":
        return QColor("#ef4444")
    if source.startswith("task.script"):
        return QColor("#7c3aed")

    palette = {
        "motor": QColor("#f97316"),
        "lick": QColor("#d946ef"),
        "water": QColor("#0ea5e9"),
        "led": QColor("#eab308"),
        "device": QColor("#64748b"),
        "session": QColor("#22c55e"),
        "app": QColor("#6366f1"),
        "task.motor_1hz": QColor("#14b8a6"),
        "task.angle_reciprocation": QColor("#f43f5e"),
        "task.script": QColor("#7c3aed"),
    }
    return palette.get(source, QColor("#8b5cf6"))
