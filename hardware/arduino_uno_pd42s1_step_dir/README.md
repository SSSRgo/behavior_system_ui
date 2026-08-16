# Arduino Uno + PD42S1 STEP/DIR test

This is a small Arduino Uno sketch for a PD42S1 / 42 closed-loop stepper driver in pulse mode.

## Files

- `arduino_uno_pd42s1_step_dir.ino`: upload this with Arduino IDE.

## Wiring

Use an external motor supply. Do not power the motor from the Arduino 5 V pin.

PLC version, common-cathode / active-high:

| Arduino Uno | PD42S1 driver |
| --- | --- |
| GND | COM |
| D2 | Step |
| D3 | Dir |
| D4 | En |
| D8 | two-photon trigger input, optional |

Common-anode / active-low, required by the optocoupler version:

| Arduino Uno | PD42S1 driver |
| --- | --- |
| 5V | COM |
| D2 | Step |
| D3 | Dir |
| D4 | En |
| D8 | two-photon trigger input, optional |

For common-anode / active-low, set this line in the sketch:

```cpp
const bool INPUT_ACTIVE_HIGH = false;
```

Motor power:

| Power supply | PD42S1 driver |
| --- | --- |
| +12 to +32 V | V+ |
| supply GND | GND |

For the PLC version, the manual says COM supports common-anode and common-cathode input. For the optocoupler version, COM only supports common-anode input from 3.3 V to 5 V.

## Driver setup

The sketch assumes:

- pulse mode
- 16 microsteps
- EN active-high

The official `42 motor/05 pulse mode control` example configures those settings through UART. You can also set them with the driver buttons/menu if available.

## Serial commands

Open Serial Monitor at `115200` baud:

| Command | Action |
| --- | --- |
| `f` | move +1 revolution |
| `r` | move -1 revolution |
| `m2.5` | move +2.5 revolutions |
| `m-0.25` | move -0.25 revolutions |
| `v120` | set speed to 120 RPM |
| `o` | move +120 degrees, then -120 degrees once |
| `p` | repeat 120 degree back-and-forth motion |
| `s` | stop repeated 120 degree motion |
| `e` | toggle enable |
| `c` | send driver setup commands through TTL serial |
| `h` | print help |

When repeated motion is running, `s` is read between swing cycles. One full +120/-120 degree cycle will finish before the stop takes effect.

## Two-photon sync trigger

Arduino `D8` outputs one TTL pulse before each full motor revolution starts inside `moveRevolutions()`.

- `f` sends one trigger pulse, then moves one revolution.
- `r` sends one trigger pulse, then moves one revolution in reverse.
- `m3` sends one trigger pulse before each of the three revolutions.
- `m2.5` sends trigger pulses before the first two full revolutions only.
- `o` sends one trigger before the +120 degree move and one trigger before the -120 degree return.
- `p` repeats the same two-trigger +120/-120 degree swing cycle until stopped with `s`.

The trigger width is controlled by:

```cpp
const unsigned int TWO_PHOTON_TRIGGER_MS = 10;
```

Arduino Uno outputs 5 V TTL. If the two-photon input expects 3.3 V or isolated input, use a level shifter, voltage divider, or optocoupler.

By default the sketch waits for a Serial Monitor command before moving. To run a slow one-revolution forward/backward startup demo, set:

```cpp
const bool RUN_DEMO_ON_STARTUP = true;
```

## Optional driver setup from Arduino code

The sketch can send PD42S1 serial commands to set:

- pulse mode
- `MICROSTEPS`
- EN active level
- DIR positive direction level

This requires the TTL version, or the correct TTL/RS485/CAN adapter for your driver version. On the side connector, the communication pins may not be printed as plain `TX` and `RX`; they may be printed as combined labels:

- `T/A/H` = `TTL_TX / 485_A / CAN_H`
- `R/B/L` = `TTL_RX / 485_B / CAN_L`

For direct TTL wiring:

| Arduino Uno | PD42S1 driver |
| --- | --- |
| D10 | T/A/H |
| D11 | R/B/L |
| GND | power/signal GND |

Arduino `D10` is receive, so it connects to the driver's transmit pin `T/A/H`. Arduino `D11` is transmit, so it connects to the driver's receive pin `R/B/L`.

To configure manually after upload, open Serial Monitor and send:

```text
c
```

To configure automatically on startup, set:

```cpp
const bool CONFIGURE_DRIVER_ON_STARTUP = true;
```

Note: Arduino Uno `SoftwareSerial` at `115200` baud is acceptable for short setup commands, but not ideal for continuous high-rate communication. For serious serial control, use a board with a second hardware UART, such as Arduino Mega.
