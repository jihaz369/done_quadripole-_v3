# JIHAZ369 QUADCOPTER SYSTEM

### Remote Controller + Flight Controller

A custom Arduino-based quadcopter control system developed under the **JIHAZ369** project.

The system is divided into two main units:

```text
┌──────────────────────────────┐
│     JIHAZ369 REMOTE          │
│                              │
│ Arduino UNO                  │
│ 1602 LCD Keypad              │
│ 4-Axis Joysticks             │
│ A/B Control Buttons          │
│ nRF24L01                     │
└──────────────┬───────────────┘
               │
               │ 2.4 GHz
               │ nRF24L01
               │
               ▼
┌──────────────────────────────┐
│     JIHAZ369 FLIGHT          │
│       CONTROLLER             │
│                              │
│ Arduino Pro Mini             │
│ MPU6050 IMU                  │
│ BMP280 Pressure Sensor       │
│ NEO-6M GPS                  │
│ nRF24L01                     │
│ ESC / Motor Outputs          │
│ Battery Monitor              │
└──────────────────────────────┘
```

---

# 1. Project Overview

The **JIHAZ369 Quadcopter System** is an experimental embedded flight-control platform based on Arduino microcontrollers and nRF24L01 wireless communication.

The system contains:

### Remote Controller

The remote controller is responsible for:

* Pilot joystick input
* Throttle control
* Roll control
* Pitch control
* Yaw control
* Flight-mode selection
* ARM / DISARM command
* RTH command
* Emergency command
* LCD status display
* Wireless control transmission
* Flight-controller telemetry reception

### Flight Controller

The flight controller is responsible for:

* Reading the MPU6050 IMU
* Reading barometric altitude
* Reading GPS data
* Receiving remote commands
* Running flight-control logic
* ARM / DISARM safety
* Failsafe handling
* RTH logic
* Safe-land logic
* Emergency handling
* Battery monitoring
* Generating ESC motor commands
* Sending telemetry back to the remote

---

# 2. System Architecture

```text
                 JIHAZ369 REMOTE
                       │
                       │
             ┌─────────▼─────────┐
             │    Arduino UNO    │
             │                   │
             │  LCD 1602         │
             │  Joysticks        │
             │  Buttons          │
             │  nRF24L01         │
             └─────────┬─────────┘
                       │
                       │ 2.4 GHz
                       │
                CONTROL PACKETS
                       │
                       ▼
             ┌───────────────────┐
             │     nRF24L01      │
             │ Flight Controller │
             └─────────┬─────────┘
                       │
                       ▼
             ┌───────────────────┐
             │ Arduino Pro Mini  │
             │                   │
             │ MPU6050            │
             │ BMP280             │
             │ GPS                │
             │ Battery Monitor    │
             │ Flight Logic       │
             └─────────┬─────────┘
                       │
                 ESC OUTPUTS
                       │
             ┌─────────┼─────────┐
             ▼         ▼         ▼         ▼
            M1        M2        M3        M4


              TELEMETRY PACKETS
                       │
                       │
                       ▼
                 REMOTE LCD
```

---

# 3. Remote Controller

## Hardware

The remote controller uses:

* Arduino UNO
* 1602 LCD Keypad Shield
* nRF24L01
* 4-axis joystick inputs
* External A/B buttons
* USB connection for programming

---

# 4. Remote Controller Pinout

## LCD 1602 Keypad Shield

| LCD Signal | Arduino UNO |
| ---------- | ----------: |
| RS         |          D8 |
| EN         |          D9 |
| D4         |          D4 |
| D5         |          D5 |
| D6         |          D6 |
| D7         |          D7 |
| Keypad     |          A0 |

---

## nRF24L01

| nRF24 | Arduino UNO |
| ----- | ----------: |
| CE    |          D2 |
| CSN   |          D3 |
| MOSI  |         D11 |
| MISO  |         D12 |
| SCK   |         D13 |
| VCC   |        3.3V |
| GND   |         GND |

The nRF24L01 should be powered from a stable **3.3 V supply**.

---

## Joysticks

| Function | Arduino |
| -------- | ------: |
| YAW      |      A1 |
| THROTTLE |      A2 |
| ROLL     |      A3 |
| PITCH    |      A4 |

---

# 5. Remote A/B Button System

The remote uses a resistor ladder so two external buttons can share **A5**.

```text
                 +5V
                  |
                 10kΩ
                  |
                  +--------- A5
                  |
              BUTTON A
                  |
                 GND

A5 -------- 4.7kΩ -------- BUTTON B -------- GND
```

Expected ADC readings:

| State    | Approx. ADC |
| -------- | ----------: |
| Released |        1023 |
| Button A |           0 |
| Button B |        ~327 |

A5 must remain an **analog input**.

Do not use:

```cpp
INPUT_PULLUP
```

for A5.

---

# 6. Remote Button Functions

## Button A

### Short press

Changes the previewed flight mode.

```text
STABILIZE
   ↓
HOLD ALT
   ↓
FREE
   ↓
SAFE LAND
   ↓
STABILIZE
```

### Hold for 1.5 seconds

Toggles:

```text
ARM
```

or

```text
DISARM
```

The long hold is intentionally separated from the short press.

A long hold does **not** also change the flight mode.

---

# 7. Button B

Button B applies the currently previewed mode.

Example:

```text
Current:
STABILIZE

Button A:

PREVIEW MODE:
HOLD ALT

Button B:

MODE SELECTED
ARM:OFF
```

---

# 8. LCD Keypad Functions

The 1602 keypad provides additional controls.

| Key    | Function               |
| ------ | ---------------------- |
| RIGHT  | Next flight mode       |
| LEFT   | ARM / DISARM           |
| UP     | RTH                    |
| DOWN   | Emergency              |
| SELECT | Cancel RTH / Emergency |

---

# 9. Flight Modes

The system currently supports four modes.

## MODE 0 — STABILIZE

Primary stabilized flight mode.

```text
MODE_STABILIZE = 0
```

---

## MODE 1 — HOLD ALT

Altitude-hold mode.

```text
MODE_HOLD_ALT = 1
```

Uses barometric altitude information from the pressure sensor.

---

## MODE 2 — FREE

Manual/free flight mode.

```text
MODE_FREE = 2
```

---

## MODE 3 — SAFE LAND

Controlled landing mode.

```text
MODE_SAFE_LAND = 3
```

---

# 10. Throttle Calibration

The remote performs throttle calibration during startup.

The pilot should keep the throttle stick fully down.

The controller takes multiple samples:

```text
100 samples
```

and calculates the low throttle value.

Example:

```text
THR CALIBRATE
KEEP DOWN...
```

Then:

```text
THR CALIBRATED
LOW=xxx
```

The calibrated value is used to determine whether the throttle is low enough for ARM.

---

# 11. ARM Safety

The remote controller does not allow normal ARM when:

* throttle calibration has not completed
* throttle is above the ARM limit
* RTH is active
* emergency mode is active

The normal ARM throttle limit is approximately:

```text
35 / 1000
```

The flight controller also performs its own ARM safety checks.

The remote is therefore **not the only safety layer**.

---

# 12. Emergency

Emergency command:

```text
DOWN
```

sets:

```text
emergency = ON
arm = OFF
rth = OFF
```

The flight controller receives the emergency state and executes its configured emergency behavior.

---

# 13. Return To Home

RTH can be requested using:

```text
UP
```

RTH sets:

```text
rth = ON
emergency = OFF
```

The flight controller uses GPS information and its RTH logic to manage the return procedure.

The flight controller remains authoritative over the actual flight state.

---

# 14. Cancel Command

SELECT cancels:

```text
RTH
```

and:

```text
EMERGENCY
```

The controller sends both states as disabled after cancellation.

---

# 15. Joystick Processing

The joystick ADC values are converted into control values.

### Throttle

```text
0 → 1000
```

### Roll

```text
-1000 → +1000
```

### Pitch

```text
-1000 → +1000
```

### Yaw

```text
-1000 → +1000
```

A small deadzone is applied to roll, pitch and yaw.

Current deadzone:

```text
35
```

The final radio packet scales roll, pitch and yaw approximately to:

```text
-100 → +100
```

---

# 16. Flight Controller

The flight controller uses an Arduino Pro Mini ATmega328P.

Main functions:

* IMU processing
* Attitude stabilization
* Altitude processing
* GPS processing
* Radio command processing
* Motor output
* Battery monitoring
* Failsafe
* RTH
* Safe landing
* Emergency handling
* Telemetry

---

# 17. Flight Controller Hardware

## MPU6050

I2C address:

```text
0x68
```

Connections:

| MPU6050 |                         Pro Mini |
| ------- | -------------------------------: |
| SDA     |                               A4 |
| SCL     |                               A5 |
| VCC     | 3.3V / appropriate module supply |
| GND     |                              GND |

The MPU6050 provides:

* Accelerometer
* Gyroscope
* Attitude information

---

# 18. BMP280

Typical addresses:

```text
0x76
```

or:

```text
0x77
```

The pressure sensor is used for:

* Relative altitude
* Home pressure
* Altitude hold
* Landing logic
* RTH altitude logic

---

# 19. GPS

GPS module:

```text
NEO-6M
```

SoftwareSerial:

| GPS  | Pro Mini |
| ---- | -------: |
| TX   |       D4 |
| RX   |       D5 |
| Baud |     9600 |

The GPS provides:

* Position
* GPS fix
* Satellite count
* Home position
* Distance
* RTH navigation information

---

# 20. Flight Controller nRF24L01

| nRF24 | Pro Mini |
| ----- | -------: |
| CE    |       D9 |
| CSN   |      D10 |
| MOSI  |      D11 |
| MISO  |      D12 |
| SCK   |      D13 |

Radio configuration:

```text
Channel: 108
Data rate: 250 KBPS
CRC: 16 bit
PA: LOW
Auto ACK: enabled
```

---

# 21. Radio Pipes

Control packets:

```cpp
0xE8E8F0F0E1LL
```

Telemetry:

```cpp
0xE8E8F0F0E2LL
```

The remote transmits control packets and listens for telemetry.

---

# 22. Control Packet

The remote sends:

```cpp
struct ControlPacket
{
  uint16_t throttle;

  int16_t roll;
  int16_t pitch;
  int16_t yaw;

  uint8_t mode;
  uint8_t arm;
  uint8_t rth;
  uint8_t emergency;

  uint8_t sequence;
};
```

The packet contains:

| Field     | Purpose                |
| --------- | ---------------------- |
| throttle  | Throttle command       |
| roll      | Roll command           |
| pitch     | Pitch command          |
| yaw       | Yaw command            |
| mode      | Selected flight mode   |
| arm       | ARM state              |
| rth       | RTH command            |
| emergency | Emergency command      |
| sequence  | Packet sequence number |

---

# 23. Telemetry Packet

The flight controller sends:

```cpp
struct TelemetryPacket
{
  uint8_t mode;
  uint8_t armed;
  uint8_t link;

  uint8_t gpsFix;
  uint8_t sats;

  int16_t altitude_dm;

  uint16_t distance_m;

  uint16_t battery_cV;

  uint8_t failsafe;
  uint8_t rth;
};
```

Telemetry contains:

| Field       | Information            |
| ----------- | ---------------------- |
| mode        | Current FC mode        |
| armed       | Actual ARM state       |
| link        | Radio link state       |
| gpsFix      | GPS fix                |
| sats        | Satellite count        |
| altitude_dm | Altitude in decimeters |
| distance_m  | Distance from home     |
| battery_cV  | Battery voltage        |
| failsafe    | Failsafe state         |
| rth         | RTH state              |

---

# 24. Flight Controller Is Authoritative

An important design principle is:

```text
REMOTE COMMAND
      ↓
FLIGHT CONTROLLER
      ↓
SAFETY CHECK
      ↓
ACTUAL FLIGHT STATE
```

The remote can request ARM.

The flight controller decides whether the aircraft can actually become armed.

The remote then receives the actual state through telemetry.

Therefore:

```text
Remote ARM request
        ≠
Guaranteed FC ARM
```

The FC remains the final authority.

---

# 25. Motor Outputs

Current motor mapping:

```text
             FRONT

        M1           M2
       D7             D3

        ┌─────────────┐
        │             │
        │   FRAME     │
        │             │
        └─────────────┘

       D8             D6
        M4           M3

              REAR
```

Current mapping:

| Motor | Position    | Pin |
| ----- | ----------- | --: |
| M1    | Front Left  |  D7 |
| M2    | Front Right |  D3 |
| M3    | Rear Right  |  D6 |
| M4    | Rear Left   |  D8 |

Verify motor direction and mixer configuration before any powered flight test.

---

# 26. Flight Controller Safety States

The FC uses safety-oriented states including:

```text
SAFE
FLYING
RTH
EMERGENCY
FAILSAFE
```

The exact state transition depends on the FC firmware.

---

# 27. Failsafe

The radio communication timeout is approximately:

```text
500 ms
```

If control packets stop arriving, the FC can enter its configured failsafe behavior.

Possible safety behavior includes:

```text
REMOTE LOST
     ↓
FAILSAFE
     ↓
SAFE LAND / configured recovery
```

The exact behavior should always be verified on the bench before flight.

---

# 28. Low Battery Protection

The flight controller monitors battery voltage using an analog divider.

Current concept:

```text
100kΩ
  |
  +---- Analog input
  |
10kΩ
  |
 GND
```

The FC monitors the battery and can request/force a safe-land behavior when configured low-voltage conditions are detected.

---

# 29. RTH Safety Logic

The flight controller includes RTH-related safety logic.

The design includes considerations such as:

* GPS availability
* altitude
* distance from home
* horizontal navigation
* stabilized control
* landing conditions
* low-speed landing detection
* motor cut conditions

RTH should be tested without propellers first.

---

# 30. Safe Landing

The FC uses altitude and vertical-speed information to determine landing conditions.

Landing motor-cut logic should only occur after the configured landing conditions have been satisfied.

This prevents an immediate motor cut simply because an RTH or landing command was received.

---

# 31. PID Control

The current project configuration uses PID control concepts for:

### Roll

```text
P = 4
I = 0
D = 0.08
```

### Pitch

```text
P = 4
I = 0
D = 0.08
```

### Yaw

```text
P = 2
I = 0
D = 0.03
```

### Altitude

```text
P = 80
I = 5
D = 25
```

These values are experimental and must be tuned for the actual aircraft, motors, ESCs, frame and sensors.

---

# 32. Attitude Filtering

The flight controller uses filtering for attitude estimation.

The Kalman configuration currently uses approximately:

```text
Q_angle = 0.001
Q_bias  = 0.003
R_measure = 0.03
```

The purpose is to combine sensor information into a more stable attitude estimate.

---

# 33. Communication Timing

Remote control packets are transmitted approximately every:

```text
50 ms
```

Equivalent to:

```text
20 Hz
```

Telemetry is received asynchronously.

Remote telemetry timeout:

```text
1500 ms
```

Flight-controller radio timeout:

```text
500 ms
```

---

# 34. Remote LCD Startup Sequence

The remote displays startup information.

Typical sequence:

```text
WELCOME TO
JIHAZ369
```

Then:

```text
NRF24 RADIO
CH108 250KBPS
```

Then:

```text
FC TELEMETRY
LINK OK
```

Then joystick information:

```text
T:0 Y:0
R:0 P:0 D
```

---

# 35. Normal LCD Display

Example:

```text
T:120 Y:-5
R:3 P:10 A
```

Where:

```text
T = Throttle
Y = Yaw
R = Roll
P = Pitch
A = Armed
D = Disarmed
```

---

# 36. Mode Selection Workflow

Example:

```text
Current Mode
STABILIZE
```

Press Button A:

```text
PREVIEW MODE:
HOLD ALT
```

Press Button A again:

```text
PREVIEW MODE:
FREE
```

Press Button B:

```text
MODE SELECTED
ARM:OFF
```

The selected mode becomes the current mode.

---

# 37. ARM Workflow

Before ARM:

```text
Throttle calibrated
        +
Throttle low
        +
No emergency
        +
No RTH
```

Then:

```text
Hold Button A
     │
     │ 1.5 seconds
     ▼
ARM request
     │
     ▼
Flight Controller
     │
     ▼
Safety checks
     │
     ▼
Actual ARM state
     │
     ▼
Telemetry
```

---

# 38. Important Safety Principle

The remote controller should never be considered the only safety system.

The flight controller must independently validate:

* ARM conditions
* radio connection
* throttle
* sensor status
* battery
* GPS conditions
* failsafe state
* emergency state
* RTH conditions
* landing conditions

---

# 39. Bench Testing

**REMOVE ALL PROPELLERS before testing motors or flight-control logic.**

Recommended test sequence:

### Test 1 — Remote startup

Verify:

```text
WELCOME
NRF24 STATUS
THROTTLE CALIBRATION
```

### Test 2 — Joysticks

Verify:

```text
Throttle = 0
Yaw ≈ 0
Roll ≈ 0
Pitch ≈ 0
```

with sticks centered/down as appropriate.

### Test 3 — Radio

Verify the remote reports:

```text
LINK OK
```

when the FC is transmitting telemetry.

### Test 4 — Mode selection

Test:

```text
STABILIZE
HOLD ALT
FREE
SAFE LAND
```

### Test 5 — ARM

Keep all propellers removed.

Verify that ARM only occurs when throttle is low.

### Test 6 — DISARM

Verify Button A hold or LEFT keypad command correctly requests DISARM.

### Test 7 — Emergency

Test:

```text
DOWN
```

and verify:

```text
ARM = OFF
RTH = OFF
```

### Test 8 — RTH

Test only after GPS and navigation behavior have been independently verified.

---

# 40. Troubleshooting

## nRF24 ERROR

Check:

```text
CE
CSN
MOSI
MISO
SCK
3.3V
GND
```

Also verify that the module has a stable 3.3 V supply.

---

## FC TELEMETRY — WAITING

Check:

* FC is powered
* FC nRF24 is connected
* Remote nRF24 is connected
* channel is 108
* data rate is 250 KBPS
* pipe addresses match
* packet structures match
* radio power is stable

---

## Button A Not Working

Check the A5 voltage/ADC value.

Expected:

```text
Released ≈ 1023
Button A ≈ 0
Button B ≈ 327
```

Check the resistor values:

```text
10kΩ
4.7kΩ
```

Do not enable the internal pull-up on A5.

---

## ARM Blocked

Check:

```text
THR CALIBRATED
THROTTLE LOW
RTH OFF
EMERGENCY OFF
```

The FC may also independently reject the ARM request.

---

## GPS No Fix

Verify:

* GPS TX/RX wiring
* 9600 baud
* outdoor sky visibility
* antenna orientation
* adequate startup time

A GPS may report:

```text
Fix = 0
Satellites = 0
```

indoors.

---

# 41. Software

## Remote

Recommended:

```text
Arduino IDE
Arduino AVR Boards
RF24 library
LiquidCrystal library
SPI library
```

Main firmware:

```text
JIHAZ369_QUADCOPTER_REMOTE_V1.8
```

---

## Flight Controller

Recommended:

```text
Arduino IDE
Arduino AVR Boards
RF24 library
Servo library
SoftwareSerial
Wire
SPI
```

The FC firmware is optimized for the ATmega328P and uses direct sensor communication where appropriate to reduce memory usage.

---

# 42. Project Structure

Recommended repository structure:

```text
JIHAZ369-QUADCOPTER/
│
├── README.md
│
├── LICENSE
│
├── REMOTE/
│   └── JIHAZ369_REMOTE_V1.8/
│       └── JIHAZ369_REMOTE_V1.8.ino
│
├── FLIGHT_CONTROLLER/
│   └── JIHAZ369_FC/
│       └── JIHAZ369_FC.ino
│
├── DOCUMENTATION/
│   ├── WIRING.md
│   ├── RADIO_PROTOCOL.md
│   └── SAFETY.md
│
└── TESTS/
    ├── NRF24_TEST/
    ├── MPU6050_TEST/
    ├── BMP280_TEST/
    └── GPS_TEST/
```

---

# 43. Communication Protocol

Both devices must use identical packet structures.

```text
REMOTE
  │
  │ ControlPacket
  ▼
FLIGHT CONTROLLER
  │
  │ TelemetryPacket
  ▼
REMOTE
```

The following must remain synchronized between both firmware versions:

```text
Packet structure
Data types
Packet size
Pipe addresses
Radio channel
Data rate
CRC
Control ranges
Mode numbers
```

Changing a packet structure on one side without changing the other can cause communication problems.

---

# 44. Version

Current remote controller:

```text
JIHAZ369 REMOTE CONTROLLER
V1.8
```

Platform:

```text
Arduino UNO
```

Radio:

```text
nRF24L01
```

Current system architecture:

```text
REMOTE V1.8
      +
JIHAZ369 FLIGHT CONTROLLER
```

---

# 45. Development Status

This project is an experimental embedded flight-control platform.

Current development areas include:

* Radio communication
* Joystick control
* Flight modes
* ARM safety
* Failsafe
* GPS
* Altitude control
* RTH
* Safe landing
* Battery monitoring
* Telemetry
* Processing-based monitoring
* Sensor diagnostics

---

# 46. Safety Notice

This project involves motors, ESCs, batteries, radio systems and potentially autonomous flight functions.

Always:

* Remove propellers during development.
* Test electronics before installing propellers.
* Verify motor numbering.
* Verify motor rotation direction.
* Verify ESC behavior.
* Verify failsafe behavior.
* Verify ARM/DISARM behavior.
* Verify battery monitoring.
* Test GPS/RTH only in a suitable environment.
* Never assume software safety without testing the actual hardware.

**Bench testing without propellers is strongly recommended during firmware development.**

---

# 47. License

This project is released under the **MIT License**.

```text
MIT License

Copyright (c) 2026 JIHAZ369

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

# JIHAZ369

**JIHAZ369 QUADCOPTER SYSTEM**

```text
BUILD
TEST
LEARN
IMPROVE
```

**Remote Controller:** V1.8
**Platform:** Arduino UNO
**Flight Controller:** Arduino Pro Mini ATmega328P
**Radio:** nRF24L01
**Sensors:** MPU6050 + BMP280 + GPS
**Project:** JIHAZ369
