# Behavior System UI

Python / VS Code framework for a modular behavior-control system.

The first implemented module controls the Arduino Uno + PD42S1 motor sketch in:

```text
../arduino_uno_pd42s1_step_dir/arduino_uno_pd42s1_step_dir.ino
```

The architecture is intentionally pyControl-like:

- hardware is represented as modules
- modules emit timestamped events
- experiments save event logs and session config
- real-time motor pulses and TTL timing stay on the microcontroller
- the Python UI sends high-level commands and records events

## Install

In VS Code terminal:

```powershell
cd "E:\GPS\PhD\free moving 2p\device\behavior_system_ui"
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## Run

```powershell
python run_ui.py
```

Or use the VS Code launch configuration if this repository is opened as the workspace.

## Current Arduino command mapping

The motor UI sends these commands to the Arduino:

| UI action | Arduino command |
| --- | --- |
| Set RPM | `v<rpm>` |
| +1 Rev | `f` |
| -1 Rev | `r` |
| Move Revolutions | `m<revolutions>` |
| 120 deg Once | `o` |
| Repeat 120 deg | `p` |
| Stop Repeat | `s` |
| Toggle Enable | `e` |
| Configure Driver | `c` |

The Arduino handles precise STEP/DIR pulse timing and outputs the two-photon trigger on D8 before each full revolution starts.

## Timing Visualization

The right side of the UI contains a live timing view. Every event published on the event bus is drawn on a source lane:

- `task.motor_1hz`
- `task.angle_reciprocation`
- `motor`
- `device`
- `lick`
- `water`
- `led`
- `session`

Use the timeline window control to show the last 5 to 600 seconds. The text log below it remains the exact event stream that is saved to disk.

## Task Files

Tasks are loaded from files instead of being hard-coded into the main window. The UI supports two task-file styles:

- `.py` task scripts for flexible behavior programs
- `.json` task configs for parameter-only variants

```text
behavior_system_ui/configs/tasks/
behavior_system_ui/task_scripts/
```

The UI `Tasks` panel can refresh, browse, load, start, and stop a selected task file. This keeps the window stable even as the number of tasks grows.

Included examples:

| Config file | Behavior |
| --- | --- |
| `motor_1rev_1hz.json` | send one +1 revolution command every 1000 ms |
| `angle_60deg_1hz.json` | alternate +60 and -60 degree movements every 1000 ms |
| `script_angle_30deg_2hz.json` | run a Python script with JSON parameters |
| `task_scripts/motor_1rev_1hz.py` | scripted one-revolution task |
| `task_scripts/angle_60deg_1hz.py` | scripted angle reciprocation task |
| `task_scripts/lick_reward_template.py` | starter template for lick/reward logic |

### JSON Config Format

```json
{
  "name": "angle_60deg_1hz",
  "task_type": "angle_reciprocation",
  "description": "Alternate +60 and -60 degree movements every 1000 ms.",
  "parameters": {
    "angle_degrees": 60,
    "interval_ms": 1000,
    "immediate": true
  }
}
```

For a new parameter variant, copy an existing JSON file and edit `name` and `parameters`.

JSON can also wrap a Python script and pass parameters into it:

```json
{
  "name": "script_angle_30deg_2hz",
  "task_type": "script",
  "parameters": {
    "script_path": "../../task_scripts/angle_60deg_1hz.py",
    "angle_degrees": 30,
    "interval_ms": 500,
    "immediate": true
  }
}
```

### Python Task Script Format

For flexible behavior programs, write a Python script with `create_task(ctx)`. The `ctx` object gives the script access to modules and the event bus:

```python
from PySide6.QtCore import QObject, QTimer


class Task(QObject):
    source = "task.script.example"

    def __init__(self, ctx):
        super().__init__()
        self.ctx = ctx
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.run_trial)

    def start(self, parameters):
        self.timer.start(int(parameters.get("interval_ms", 1000)))
        self.ctx.publish(self.source, "started")

    def stop(self):
        self.timer.stop()
        self.ctx.publish(self.source, "stopped")

    def run_trial(self):
        self.ctx.motor.move_degrees(60)


def create_task(ctx):
    return Task(ctx)
```

Available context fields:

- `ctx.motor`
- `ctx.lick`
- `ctx.water`
- `ctx.led`
- `ctx.bus`
- `ctx.publish(source, event_type, **payload)`

For a new built-in task type, add a task class under `behavior_system/tasks/`, implement `start_from_config(parameters)`, then register it in `TaskRegistry`. For most experimental tasks, prefer the script format first.

## Built-in Task Types

### `motor_every_second`

Sends one full motor revolution command at a fixed interval.

Default behavior:

- interval: `1000 ms`
- command sent each cycle: `f`
- first cycle runs immediately when the task starts
- each cycle is logged as `task.motor_1hz | cycle_start`

For a true one-revolution-per-second physical motion, set motor speed to at least `60 RPM`. If the motor is slower than `60 RPM`, one revolution takes more than one second and Arduino commands can queue up.

### `angle_reciprocation`

Sends alternating positive and negative angle movements at a fixed interval.

Example:

- angle: `60 deg`
- interval: `1000 ms`
- command pattern: `+60 deg`, `-60 deg`, `+60 deg`, `-60 deg`
- Arduino command pattern: `m0.166667`, `m-0.166667`, ...

This is useful for a 1 Hz back-and-forth stimulus. The first movement starts immediately when the config is started.

Make sure the selected RPM is fast enough for the angle to finish before the next interval. For example, at `60 RPM`, a 60 degree movement takes about `167 ms`, so a `1000 ms` interval has plenty of margin.

## Adding Modules

Add a new hardware module by subclassing `HardwareModule`, then add a UI widget for it. The project already includes starter modules for motor, lick, water reward, and LED.

Example shape:

```python
class WaterModule(HardwareModule):
    info = ModuleInfo(name="water", display_name="Water")

    def reward(self, duration_ms: int) -> None:
        self.send(f"w{duration_ms}", event_type="reward", duration_ms=duration_ms)
```

Then add `WaterWidget` in `behavior_system/ui/module_widgets.py` and register it in `MainWindow.build_module_tabs()`.

## Logs

When a session is started, logs are written under:

```text
behavior_system_ui/logs/<timestamp>_<animal>_<task>/
```

Each session contains:

- `session_config.json`
- `events.csv`
- `events.jsonl`

## Timing Note

Use the Arduino/Teensy/ESP32 side for millisecond-level actions: motor pulses, lick edges, valve pulses, LED pulses, camera TTL, and two-photon sync.

Use the Python UI for high-level commands, configuration, status display, and logging.
