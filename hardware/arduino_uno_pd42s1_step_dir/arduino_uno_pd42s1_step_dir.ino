/*
  Arduino Uno STEP/DIR control for PD42S1 / 42 closed-loop stepper driver.

  Wiring option A, PLC version common-cathode / active-high:
    Uno GND  -> driver COM
    Uno D2   -> driver Step
    Uno D3   -> driver Dir
    Uno D4   -> driver En

  Wiring option B, common-anode / active-low, required by optocoupler version:
    Uno 5V   -> driver COM
    Uno D2   -> driver Step
    Uno D3   -> driver Dir
    Uno D4   -> driver En
    Set INPUT_ACTIVE_HIGH to false.

  Optional driver setup serial wiring, TTL version only:
    Uno D10  -> driver T/A/H pin, used as TX on TTL version
    Uno D11  -> driver R/B/L pin, used as RX on TTL version
    Uno GND  -> driver GND

  Optional two-photon sync:
    Uno D8   -> two-photon trigger / digital input
    Uno GND  -> two-photon digital input ground

  Motor power:
    External 12-32 V supply -> driver V+ and GND.
    Do not power the motor from the Arduino 5 V pin.

  Driver setup:
    Set CONFIGURE_DRIVER_ON_STARTUP to true if you want the Uno to configure
    pulse mode, microsteps, EN level, and DIR level through the driver's TTL
    serial pins on startup.

  Serial Monitor commands at 115200 baud:
    f       Move +1 revolution
    r       Move -1 revolution
    m2.5    Move +2.5 revolutions
    m-0.25  Move -0.25 revolutions
    v120    Set speed to 120 RPM
    o       Move +120 degrees, then -120 degrees once
    p       Start repeated 120 degree back-and-forth motion
    s       Stop repeated motion
    e       Toggle enable
    c       Send driver setup commands
    h       Print help
*/

#include <SoftwareSerial.h>

const byte STEP_PIN = 2;
const byte DIR_PIN = 3;
const byte EN_PIN = 4;
const byte TWO_PHOTON_TRIGGER_PIN = 8;
const byte LED_PIN = 13;

const byte DRIVER_RX_PIN = 10;          // Uno RX, connect to driver T/A/H pin on TTL version
const byte DRIVER_TX_PIN = 11;          // Uno TX, connect to driver R/B/L pin on TTL version

const long MOTOR_FULL_STEPS_PER_REV = 200;  // 1.8 degree motor
const int MICROSTEPS = 16;                  // Must match driver setting
const float SWING_ANGLE_DEGREES = 120.0;    // Back-and-forth motion angle
const unsigned int SWING_PAUSE_MS = 0;       // Pause at each end
const unsigned int TWO_PHOTON_TRIGGER_MS = 10;

const bool INPUT_ACTIVE_HIGH = true;        // false for common-anode / active-low wiring
const bool RUN_DEMO_ON_STARTUP = false;     // Set true for one slow CW/CCW test after reset
const bool CONFIGURE_DRIVER_ON_STARTUP = false;

const byte DRIVER_ADDR = 1;
const unsigned long DRIVER_BAUD = 115200;

const byte FRAME_HEAD = 0xC5;
const byte FRAME_TAIL = 0x5C;
const byte FCT_SET_STEP = 0x65;
const byte FCT_SET_DIR_LEVEL = 0x6D;
const byte FCT_SET_EN_LEVEL = 0x6E;
const byte FCT_PULSES_MODE = 0xF4;

float currentRpm = 60.0;
bool motorEnabled = false;
bool repeatSwing = false;

SoftwareSerial driverSerial(DRIVER_RX_PIN, DRIVER_TX_PIN);

float absoluteFloat(float value)
{
  return value < 0.0 ? -value : value;
}

byte checksum(const byte *data, byte length)
{
  byte sum = 0;
  for (byte i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

void sendDriverCommand(byte *cmd, byte payloadLength)
{
  byte checksumIndex = payloadLength;
  byte tailIndex = payloadLength + 1;
  cmd[checksumIndex] = checksum(cmd, payloadLength);
  cmd[tailIndex] = FRAME_TAIL;

  driverSerial.write(cmd, payloadLength + 2);
  driverSerial.flush();
  delay(80);
}

void driverSimpleCommand(byte functionCode)
{
  byte cmd[5] = {FRAME_HEAD, DRIVER_ADDR, functionCode, 0x00, FRAME_TAIL};
  sendDriverCommand(cmd, 3);
}

void driverSetByte(byte functionCode, byte value)
{
  byte cmd[6] = {FRAME_HEAD, DRIVER_ADDR, functionCode, value, 0x00, FRAME_TAIL};
  sendDriverCommand(cmd, 4);
}

void driverSetStep(unsigned int microsteps)
{
  byte cmd[7] = {
    FRAME_HEAD,
    DRIVER_ADDR,
    FCT_SET_STEP,
    (byte)((microsteps >> 8) & 0xFF),
    (byte)(microsteps & 0xFF),
    0x00,
    FRAME_TAIL
  };
  sendDriverCommand(cmd, 5);
}

void configureDriver()
{
  driverSerial.begin(DRIVER_BAUD);
  driverSerial.listen();

  Serial.println(F("Configuring PD42S1 through TTL serial..."));
  driverSimpleCommand(FCT_PULSES_MODE);          // Pulse mode
  driverSetStep(MICROSTEPS);                     // 1-256 microsteps
  driverSetByte(FCT_SET_EN_LEVEL, INPUT_ACTIVE_HIGH ? 1 : 0);
  driverSetByte(FCT_SET_DIR_LEVEL, 0);           // DIR high = positive direction
  Serial.println(F("Driver config commands sent."));
}

int driverLevel(bool active)
{
  if (INPUT_ACTIVE_HIGH) {
    return active ? HIGH : LOW;
  }
  return active ? LOW : HIGH;
}

void setEnable(bool enabled)
{
  motorEnabled = enabled;
  digitalWrite(EN_PIN, driverLevel(enabled));
}

void setDirection(bool forward)
{
  digitalWrite(DIR_PIN, driverLevel(forward));
}

unsigned long pulsesPerRevolution()
{
  return (unsigned long)MOTOR_FULL_STEPS_PER_REV * (unsigned long)MICROSTEPS;
}

void triggerTwoPhoton()
{
  digitalWrite(TWO_PHOTON_TRIGGER_PIN, HIGH);
  delay(TWO_PHOTON_TRIGGER_MS);
  digitalWrite(TWO_PHOTON_TRIGGER_PIN, LOW);
  Serial.println(F("Two-photon trigger"));
}

unsigned long halfPeriodUsForRpm(float rpm)
{
  const float stepsPerMinute = rpm * MOTOR_FULL_STEPS_PER_REV * MICROSTEPS;
  const float stepsPerSecond = stepsPerMinute / 60.0;
  unsigned long halfPeriod = (unsigned long)(500000.0 / stepsPerSecond);

  // Keep the pulse visible to the driver and avoid zero-delay pulses.
  if (halfPeriod < 5) {
    halfPeriod = 5;
  }
  return halfPeriod;
}

void stepOnce(unsigned long halfPeriodUs)
{
  digitalWrite(STEP_PIN, driverLevel(true));
  digitalWrite(LED_PIN, HIGH);
  delayMicroseconds(halfPeriodUs);
  digitalWrite(STEP_PIN, driverLevel(false));
  digitalWrite(LED_PIN, LOW);
  delayMicroseconds(halfPeriodUs);
}

void moveRevolutions(float revolutions)
{
  if (revolutions == 0.0) {
    return;
  }

  setEnable(true);
  setDirection(revolutions > 0.0);
  delay(10);

  unsigned long oneRevPulses = pulsesPerRevolution();
  unsigned long pulses = (unsigned long)(absoluteFloat(revolutions) * oneRevPulses + 0.5);
  unsigned long halfPeriodUs = halfPeriodUsForRpm(currentRpm);
  unsigned long fullRevolutions = pulses / oneRevPulses;
  unsigned long nextTriggerPulse = 0;
  unsigned long triggersSent = 0;

  Serial.print(F("Move rev="));
  Serial.print(revolutions, 3);
  Serial.print(F(", pulses="));
  Serial.print(pulses);
  Serial.print(F(", rpm="));
  Serial.println(currentRpm, 1);
  triggerTwoPhoton();
  for (unsigned long i = 0; i < pulses; i++) {
    // if (triggersSent < fullRevolutions && i == nextTriggerPulse) {
    //   triggerTwoPhoton();
    //   triggersSent++;
    //   nextTriggerPulse += oneRevPulses;
    // }

    stepOnce(halfPeriodUs);
  }
}

void moveDegrees(float degrees)
{
  moveRevolutions(degrees / 360.0);
}

void swing120Once()
{
  Serial.println(F("Swing +120 deg"));
  triggerTwoPhoton();
  moveDegrees(SWING_ANGLE_DEGREES);
  delay(SWING_PAUSE_MS);

  Serial.println(F("Swing -120 deg"));
  triggerTwoPhoton();
  moveDegrees(-SWING_ANGLE_DEGREES);
  delay(SWING_PAUSE_MS);
}

void printHelp()
{
  Serial.println();
  Serial.println(F("PD42S1 Arduino Uno STEP/DIR control"));
  Serial.println(F("Commands:"));
  Serial.println(F("  f       +1 revolution"));
  Serial.println(F("  r       -1 revolution"));
  Serial.println(F("  m2.5    move +2.5 revolutions"));
  Serial.println(F("  m-0.25  move -0.25 revolutions"));
  Serial.println(F("  v120    set speed to 120 RPM"));
  Serial.println(F("  o       swing 120 degrees once"));
  Serial.println(F("  p       repeat 120 degree swing"));
  Serial.println(F("  s       stop repeated swing"));
  Serial.println(F("  e       toggle enable"));
  Serial.println(F("  c       send driver setup commands"));
  Serial.println(F("  h       help"));
  Serial.println();
}

void handleCommand(String command)
{
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  char op = command.charAt(0);

  Serial.print(F("> "));
  Serial.println(command);

  if (op == 'f') {
    moveRevolutions(1.0);
  } else if (op == 'r') {
    moveRevolutions(-1.0);
  } else if (op == 'm') {
    moveRevolutions(command.substring(1).toFloat());
  } else if (op == 'o') {
    swing120Once();
  } else if (op == 'p') {
    repeatSwing = true;
    Serial.println(F("Repeated 120 degree swing ON"));
  } else if (op == 's') {
    repeatSwing = false;
    Serial.println(F("Repeated 120 degree swing OFF"));
  } else if (op == 'v') {
    float rpm = command.substring(1).toFloat();
    if (rpm > 0.0 && rpm <= 600.0) {
      currentRpm = rpm;
      Serial.print(F("RPM set to "));
      Serial.println(currentRpm, 1);
    } else {
      Serial.println(F("Use a speed from 0.1 to 600 RPM for this simple sketch."));
    }
  } else if (op == 'e') {
    setEnable(!motorEnabled);
    Serial.print(F("Enable = "));
    Serial.println(motorEnabled ? F("ON") : F("OFF"));
  } else if (op == 'c') {
    configureDriver();
  } else {
    printHelp();
  }
}

void setup()
{
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(TWO_PHOTON_TRIGGER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(STEP_PIN, driverLevel(false));
  digitalWrite(DIR_PIN, driverLevel(false));
  digitalWrite(TWO_PHOTON_TRIGGER_PIN, LOW);
  setEnable(false);

  Serial.begin(115200);
  delay(500);
  printHelp();

  if (CONFIGURE_DRIVER_ON_STARTUP) {
    configureDriver();
  }

  if (RUN_DEMO_ON_STARTUP) {
    Serial.println(F("Startup demo in 2 seconds..."));
    delay(2000);
    moveRevolutions(1.0);
    delay(500);
    moveRevolutions(-1.0);
    setEnable(false);
    Serial.println(F("Demo done."));
  }
}

void loop()
{
  if (Serial.available() > 0) {
    char c = Serial.read();

    if (c == 'f' || c == 'F') {
      handleCommand("f");
    } else if (c == 'r' || c == 'R') {
      handleCommand("r");
    } else if (c == 'e' || c == 'E') {
      handleCommand("e");
    } else if (c == 'c' || c == 'C') {
      handleCommand("c");
    } else if (c == 'o' || c == 'O') {
      handleCommand("o");
    } else if (c == 'p' || c == 'P') {
      handleCommand("p");
    } else if (c == 's' || c == 'S') {
      handleCommand("s");
    } else if (c == 'h' || c == 'H' || c == '?') {
      handleCommand("h");
    } else if (c == 'm' || c == 'M' || c == 'v' || c == 'V') {
      String command;
      command += c;
      command += Serial.readStringUntil('\n');
      handleCommand(command);
    }
  }

  if (repeatSwing) {
    swing120Once();
  }
}
