# Behavior System UI

Python UI plus an Arduino Uno behavioral controller for synchronized experiments.

## Production architecture

The production firmware is in
[`hardware/arduino_uno_behavior_controller/`](hardware/arduino_uno_behavior_controller/).
The Uno is the behavioral master clock: it owns trial states, STEP pulse generation,
servo pulses, sensor/frame edge capture, and TTL events. The PC only sends
configuration plus `ARM`, `START`, and `STOP`, displays status, and writes logs.

```text
Python UI                    Arduino Uno (master)
configuration  -----------> state machine
ARM/START/STOP ------------> Timer1 actuator scheduler
status/events  <------------ GPIO interrupts + fixed event queue
CSV/JSONL logging            micros() timestamps
```

The original blocking motor sketch remains in
`hardware/arduino_uno_pd42s1_step_dir/` as a movement/wiring reference. It has
blocking delays and no frame synchronization, so it is not production firmware.

See the [Uno controller documentation](hardware/arduino_uno_behavior_controller/README.md)
for the exact pin map, electrical rules, command/event protocol, power layout,
resource limits, and bench-test procedure.

## Install and run

From this repository:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python run_ui.py
```

Select `configs/tasks/arduino_brush_trial.json` in the Tasks panel. Production task
loading accepts `arduino_experiment` configs only. Older Python/QTimer examples are
kept as reference files but are intentionally rejected by the production selector,
because Windows timers must not schedule experiment events.

## Motor controls

The motor UI maps to readable, non-blocking Uno commands:

| UI action | Uno command |
| --- | --- |
| Set RPM | `SET STEPPER_A_SPEED_SPS <steps/s>` |
| +1 Rev | `MOVE STEPPER_A 3200` |
| -1 Rev | `MOVE STEPPER_A -3200` |
| Move Revolutions | `MOVE STEPPER_A_REV <revolutions>` |
| 120 deg Once | `SWING ONCE` |
| Repeat 120 deg | `SWING START` |
| Stop Repeat | `SWING STOP` |
| Toggle Enable | `ENABLE STEPPER_A TOGGLE` |

The legacy one-letter commands remain available for bench use. Driver configuration
command `c` is rejected; configure the PD42S1 outside an experiment instead of using
timing-disruptive `SoftwareSerial` on the Uno.

## Task configuration

The production config contains parameters only; it does not contain a PC-side timer:

```json
{
  "name": "arduino_brush_trial",
  "task_type": "arduino_experiment",
  "parameters": {
    "brush_delay_us": 2000000,
    "brush_duration_us": 300000,
    "trial_duration_us": 10000000,
    "iti_us": 2000000,
    "trial_count": 10,
    "servo_move_us": 150000,
    "servo_home_deg": 90,
    "servo_stim_deg": 120,
    "event_pulse_us": 5000
  }
}
```

Starting it sends `SET ...` lines once, followed by `ARM` and `START`. Trial timing
then continues on the Uno even if the GUI is briefly busy.

## Logs

Sessions are written under:

```text
logs/<timestamp>_<animal>_<task>/
```

Each session contains `session_config.json`, `events.csv`, and `events.jsonl`.
Event rows include:

- PC UTC time, used as wall-clock metadata;
- `master_timestamp_us`, the rollover-unwrapped Uno time used for alignment;
- trial number and event value.

Both `FRAME` messages and behavioral `EVENT` messages use the same Uno `micros()`
clock. The decoder extends the raw 32-bit counter across its approximately
71.6-minute rollover.

## Verification

Run the host tests without creating bytecode files:

```powershell
python -B -m unittest discover -s tests -v
```

Build the actual Uno target:

```powershell
pio run --project-dir hardware/arduino_uno_behavior_controller
```

The verified release build uses 20,886 bytes of Flash (64.8%) and 1,013 bytes of
static SRAM (49.5%), leaving 1,035 bytes for the stack and runtime state.
