# Arduino Uno behavior master controller

This is the production, non-blocking firmware for an Arduino Uno
(ATmega328P, 16 MHz, 2 KB SRAM, 5 V GPIO). The older sketch in
`../arduino_uno_pd42s1_step_dir/` is preserved only as a verified movement/wiring
reference.

## Timing architecture

```text
PC: configure, ARM/START/STOP, save data
                       |
                       | USB serial (not a timing source)
                       v
Arduino Uno master clock: micros()
  |-- non-blocking experiment state machine
  |-- Timer1 25 kHz scheduler
  |     |-- two independent STEP channels
  |     `-- two 50 Hz servo pulse channels
  |-- INT0: two-photon frame/exposure input
  |-- INT1: camera frame/exposure input
  |-- PCINT1: brush contact + lick input
  |-- non-blocking event/camera TTL outputs
  `-- 32-slot fixed event queue -> bounded serial writer
```

There is no `delay()`, `delayMicroseconds()`, dynamic `String`, or synchronous motor
loop in production source. Actuator callbacks do not print serial data. Interrupts
capture timestamps and enqueue compact records; formatting occurs in `loop()`.

The experiment state sequence is:

```text
IDLE -> ARMED -> TRIAL_START -> PRE_STIM -> STIMULUS
     -> POST_STIM -> ITI -> NEXT_TRIAL -> TRIAL_START / IDLE
```

All deadlines use rollover-safe unsigned subtraction. Raw protocol timestamps are
32-bit `micros()` values (4 us resolution on a 16 MHz Uno); the Python decoder
extends rollover to monotonic 64-bit `master_timestamp_us`.

### Actuator timing

Timer1 runs in CTC mode at 25 kHz, so scheduling resolution is 40 us. Both steppers
use independent phase accumulators and may run concurrently at different speeds.
STEP high and low times are each at least one tick, setting a hard limit of
12,500 steps/s. Speeds are average rates; individual edge intervals are quantized to
40 us. The same ISR generates two independent 50 Hz servo signals with 40 us pulse
width resolution (1,000–2,000 us defaults).

With the default 3,200 microsteps/revolution, 12,500 steps/s is about 234.4 RPM.

Do not include the standard Arduino `Servo` library or reconfigure Timer1. Timer1
PWM on D9/D10 is unavailable; this firmware uses those pins as a digital enable and
a custom servo output.

## Build and upload

Install PlatformIO, connect the Uno, then run from the repository root:

```powershell
pio run --project-dir hardware/arduino_uno_behavior_controller
pio run --project-dir hardware/arduino_uno_behavior_controller --target upload
pio device monitor --baud 115200
```

Verified release size:

```text
Flash: 20,886 / 32,256 bytes (64.8%)
SRAM:   1,013 /  2,048 bytes (49.5%)
```

The 1,035-byte SRAM margin is intentional. Keep using fixed storage and recheck the
linker size after every feature change.

## Pin map

D0/D1 are reserved for USB serial and must not be reused.

| Function | Uno pin | Direction | Notes |
| --- | ---: | --- | --- |
| Two-photon FRAME/EXPOSURE | D2 | input / INT0 | Rising-edge timestamp |
| Camera FRAME/EXPOSURE | D3 | input / INT1 | Rising-edge timestamp |
| Stepper A STEP | D4 | output | PD42S1 logic input |
| Stepper A DIR | D5 | output | PD42S1 logic input |
| Stepper A EN | D6 | output | Polarity in `config.h` |
| Stepper B STEP | D7 | output | Independent scheduler channel |
| Stepper B DIR | D8 | output |  |
| Stepper B EN | D9 | output | Do not use Timer1 PWM |
| Servo A | D10 | output | Brush servo, external 5–6 V supply |
| Servo B | D11 | output | External 5–6 V supply |
| EVENT TTL OUT | D12 | output | Default 5 ms HIGH pulse |
| Status LED | D13 | output | Reserved |
| Brush contact | A0 | input / PCINT8 | Active-high requires external pulldown |
| Lick sensor | A1 | input / PCINT9 | Active-high requires external pulldown |
| Camera trigger | A2 | output | Default 1 ms HIGH pulse |
| Reward valve | A3 | reserved output | Held LOW; no valve driver implemented |

The sensor pin-change ISR is specifically configured for A0/A1. Changing those two
pins requires updating both `config.h` and the AVR PCINT register/vector setup in
`SensorManager.cpp`.

## Electrical interface and power

Uno GPIO operates at 5 V:

- never drive an input below GND or above 5 V;
- a normal 3.3 V TTL source is above the ATmega328P digital HIGH threshold, but
  verify signal levels and noise margin on the actual equipment;
- if the microscope/camera accepts only 3.3 V, level-shift or divide Uno outputs
  D12/A2 before connection;
- active-high A0/A1 sensors need external pulldown resistors because the Uno has no
  internal pulldown;
- share signal ground only when equipment manuals permit it; otherwise use suitable
  digital isolation.

```text
24 V external supply  -> stepper drivers -> stepper motors
5–6 V external supply -> servo power
USB / regulated input -> Arduino Uno
all compatible signal grounds -> common/star ground
```

The Uno supplies logic only. Never power a stepper, driver power stage, MG90S servo,
or solenoid from an Uno GPIO or its 5 V regulator. Add local servo bulk capacitance,
decoupling, flyback protection for inductive loads, and keep high-current wiring away
from frame/sensor signals.

## Serial protocol

Commands are newline-terminated ASCII. Parsing uses a fixed 96-byte buffer and
consumes at most 32 received bytes per `loop()` call.

### PC to Uno

```text
SET BRUSH_DELAY_US 2000000
SET BRUSH_DURATION_US 300000
SET TRIAL_DURATION_US 10000000
SET ITI_US 2000000
SET TRIAL_COUNT 10
SET SERVO_MOVE_US 150000
SET SERVO_A_HOME 90
SET SERVO_A_STIM 120
SET EVENT_PULSE_US 5000
ARM
START
STOP
STATUS

SET STEPPER_A_SPEED_SPS 3200
SET STEPPER_B_SPEED_SPS 3200
MOVE STEPPER_A 3200
MOVE STEPPER_B -1600 6400
MOVE STEPPER_A_REV 1.5
MOVE SERVO_A 120 150000
MOVE SERVO_B 45
ENABLE STEPPER_A ON

CAMERA SINGLE
CAMERA START 33333
CAMERA STOP

TEST_SYNC 60 1000000
SIM_FRAME 1000 33333
```

Legacy `f`, `r`, `m<revolutions>`, `v<rpm>`, `o`, `p`, `s`, and `e` commands are
non-blocking and retained for bench compatibility. `c` is rejected: the Uno has only
one hardware UART, already used by USB serial, and `SoftwareSerial` would disturb
timing. Preconfigure the driver or use dedicated external hardware. `HOME` is
rejected until a real home/limit sensor and safe direction are defined.

Commands are intended for sparse control traffic, not continuous streaming. Send
configuration before `ARM`; rejected commands return an `ERR` line.

### Uno to PC

```text
EVENT,183472913,23,BRUSH_COMMAND,120
FRAME,16910,183450001
STATUS,ARMED,TRIAL,0,QUEUE,0,FRAMES,0
ACK,START
ERR,START_REQUIRES_ARMED_VALID_CONFIG
```

`EVENT` fields are raw master timestamp, trial number, event name, and value.
`FRAME` fields are frame number and raw master timestamp. `ACK`, `ERR`, and `STATUS`
are control messages and do not enter the event queue.

The queue has 32 slots (31 usable). Serial output is bounded to two records per loop
and only starts a record when the 64-byte Uno TX buffer has enough free space. If a
burst exceeds queue capacity, `EVENT_QUEUE_OVERFLOW` reports loss explicitly. At
115200 baud, reducing the mean event rate is preferable to enlarging the queue,
because SRAM is limited.

## Event table

Numeric IDs are stable in `src/events.h`; names are sent on the wire.

| ID | Name | Value |
| ---: | --- | --- |
| 1 / 2 | `SESSION_START` / `SESSION_END` | start flag / stop reason |
| 3 | `STATE_ENTER` | numeric experiment state |
| 10 / 11 | `TRIAL_START` / `TRIAL_END` | start flag / 0 |
| 20 / 21 / 22 | `BRUSH_COMMAND` / `BRUSH_CONTACT` / `BRUSH_END` | angle / level / home angle |
| 30 / 31 | `MOTOR_A_START` / `MOTOR_A_STOP` | requested steps / final position |
| 32 / 33 | `MOTOR_B_START` / `MOTOR_B_STOP` | requested steps / final position |
| 40 / 41 | `SERVO_A_START` / `SERVO_A_STOP` | target / final angle |
| 42 / 43 | `SERVO_B_START` / `SERVO_B_STOP` | target / final angle |
| 50 / 51 | `REWARD_ON` / `REWARD_OFF` | reserved |
| 60 / 61 | `SENSOR_ON` / `SENSOR_OFF` | digital level |
| 62 / 63 | `LICK_ON` / `LICK_OFF` | digital level |
| 70 | `SYNC_OUT` | source event ID |
| 71 | `TWO_PHOTON_FRAME` | emitted as `FRAME`; frame number |
| 80 / 81 | `CAMERA_TRIGGER` / `CAMERA_FRAME` | level / frame number |
| 90 / 91 / 92 | `TEST_SYNC_START` / `TEST_SYNC_PULSE` / `TEST_SYNC_END` | interval / lateness / count |
| 100 | `EVENT_QUEUE_OVERFLOW` | records lost since prior report |

`BRUSH_COMMAND` timestamps the commanded motion. `BRUSH_CONTACT` timestamps the
physical sensor edge and is the preferred physical stimulus onset. That contact ISR
also raises EVENT OUT immediately; two events closer than the configured pulse width
extend one HIGH period, because one wire cannot encode a second rising edge while it
is already HIGH.

## Two-photon and camera wiring

```text
Uno D12 EVENT OUT -> level shifter/divider if required -> microscope event input
Uno GND ------------------------------------------------> compatible digital ground

microscope frame output -> Uno D2 (INT0)
camera frame output     -> Uno D3 (INT1)
Uno A2 camera trigger   -> level shifter/divider if required -> camera trigger input
```

Confirm voltage, edge polarity, required pulse width, and ground/isolation rules in
each instrument manual before connecting. Do not connect two actively driven outputs
together.

## Bench synchronization tests

1. With no animals attached, send `TEST_SYNC 60 1000000`. D12 emits 60 nominal 1 Hz
   pulses. `TEST_SYNC_PULSE.value` is main-loop scheduling lateness in microseconds.
2. For a 5 V loopback test, disconnect all external sources, wire D12 to D2, run
   `TEST_SYNC`, and verify exactly one sequential `FRAME` for every pulse.
3. `SIM_FRAME 1000 33333` checks frame numbering, queueing, rollover decoding, and
   serial parsing without wiring. `SIM_FRAME 100000 1` intentionally stresses queue
   overflow handling; it is not an electrical latency test.
4. While `TEST_SYNC` runs, drive both steppers, both servos, camera triggers, and
   frame inputs. Check for missing frame numbers and `EVENT_QUEUE_OVERFLOW`.
5. Use an oscilloscope or logic analyzer for real jitter measurements. Capture D12
   and a reference/frame signal, then report peak-to-peak, standard deviation/RMS,
   and percentile edge error. Logged scheduling lateness does not include cable,
   level-shifter, or instrument-input latency.

Timer1 interrupts every 40 us and therefore bounds normal STEP/servo scheduling.
External interrupts wait if another ISR is already executing; validate the final
hardware under worst-case simultaneous frame and sensor activity.

## Extension limits

The Uno pin map, Timer1 channels, and SRAM budget are intentionally full. A small,
low-rate digital output may be added only after rechecking pins, timing, and linked
SRAM. Do not add a third stepper, a third servo, large buffers, dynamic allocation,
or another serial protocol on this target. For those requirements, move to a board
with more timers, interrupt-capable pins, UARTs, and SRAM.
