from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event


StateHandler = Callable[[Event], str | None]


@dataclass
class State:
    name: str
    on_event: StateHandler


@dataclass
class StateMachine:
    name: str
    bus: EventBus
    states: dict[str, State] = field(default_factory=dict)
    current_state: str = "idle"
    running: bool = False

    def add_state(self, state: State) -> None:
        self.states[state.name] = state

    def start(self, initial_state: str = "idle") -> None:
        self.current_state = initial_state
        self.running = True
        self.bus.publish(Event(source=self.name, event_type="state_enter", payload={"state": initial_state}))

    def stop(self) -> None:
        self.running = False
        self.bus.publish(Event(source=self.name, event_type="state_machine_stop", payload={"state": self.current_state}))

    def handle(self, event: Event) -> None:
        if not self.running:
            return

        state = self.states.get(self.current_state)
        if state is None:
            return

        next_state = state.on_event(event)
        if next_state and next_state != self.current_state:
            old_state = self.current_state
            self.current_state = next_state
            self.bus.publish(
                Event(
                    source=self.name,
                    event_type="state_transition",
                    payload={"from": old_state, "to": next_state, "event": event.event_type},
                )
            )
