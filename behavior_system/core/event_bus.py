from __future__ import annotations

from PySide6.QtCore import QObject, Signal

from behavior_system.core.events import Event


class EventBus(QObject):
    event_published = Signal(object)

    def publish(self, event: Event) -> None:
        self.event_published.emit(event)
