# JIHAZ369 QUADCOPTER REMOTE CONTROLLER

## V1.8 — A5 Shared A/B Button System

> **JIHAZ369 Flight Control System**
> Arduino UNO + 1602 LCD Keypad Shield + nRF24L01 + Joysticks

---

## ⚠️ SAFETY WARNING

**BENCH TEST ONLY — REMOVE ALL PROPELLERS BEFORE POWERING THE FLIGHT CONTROLLER.**

This project controls a quadcopter flight controller and contains ARM, throttle, RTH, emergency, and motor-control functionality.

Do **not** perform initial software or radio testing with propellers installed.

Always verify:

* Throttle is at minimum before ARM
* ARM/DISARM behavior
* Emergency behavior
* RTH behavior
* Radio failsafe behavior
* Telemetry communication
* Motor direction and motor mapping
* Sensor operation
* Battery monitoring

---

# 1. Project Overview

The **JIHAZ369 Quadcopter Remote Controller V1.8** is an Arduino UNO based transmitter designed to communicate with the JIHAZ369 quadcopter flight controller using an **nRF24L01+** radio module.

The controller provides:

* Four-axis joystick control
* Throttle calibration
* ARM / DISARM control
* Four flight modes
* Return-To-Home command
* Emergency command
* RTH / emergency cancellation
* FC telemetry reception
* Radio-link monitoring
* 1602 LCD user interface
* Shared A/B button input using a resistor ladder
* LCD keypad backup controls
* 20 Hz control packet transmission

---

# 2. Main Hardware

| Component        | Hardware                      |
| ---------------- | ----------------------------- |
| MCU              | Arduino UNO                   |
| Display          | 1602 LCD Keypad Shield        |
| Radio            | nRF24L01 / nRF24L01+          |
| Joysticks        | 4-axis analog joystick inputs |
| External Buttons | 2-button resistor ladder      |
| Communication    | 2.4 GHz nRF24L01              |
| Protocol         | RF24                          |
| Radio Channel    | 108                           |
| Data Rate        | 250 KBPS                      |
| CRC              | 16-bit                        |
| PA Level         | LOW                           |

---

# 3. Pin Configuration

## nRF24L01

| nRF24L01 | Arduino UNO |
| -------- | ----------: |
| CE       |          D2 |
| CSN      |          D3 |
| MOSI     |         D11 |
| MISO     |         D12 |
| SCK      |         D13 |
| VCC      |        3.3V |
| GND      |         GND |

> **Important:** The nRF24L01 should be powered from a stable **3.3 V supply**. Do not connect the radio VCC to 5 V.

---

## 1602 LCD Keypad Shield

| LCD    | Arduino UNO |
| ------ | ----------: |
| RS     |          D8 |
| EN     |          D9 |
| D4     |          D4 |
| D5     |          D5 |
| D6     |          D6 |
| D7     |          D7 |
| Keypad |          A0 |

---

## Joysticks

| Control  | Arduino UNO |
| -------- | ----------: |
| YAW      |          A1 |
| THROTTLE |          A2 |
| ROLL     |          A3 |
| PITCH    |          A4 |

---

# 4. A5 Shared A/B Button System

V1.8 uses **A5 as an analog resistor-ladder input** for two external buttons.

### Wiring

```text
             +5V
              |
             10kΩ
              |
              +---------- A5
              |
          BUTTON A
              |
             GND


A5 -------- 4.7kΩ -------- BUTTON B -------- GND
```

### Expected ADC Values

| State    | Approx. ADC |
| -------- | ----------: |
| Released |       ~1023 |
| Button A |          ~0 |
| Button B |        ~327 |

The firmware uses ADC ranges rather than requiring two separate digital pins.

### A5 Configuration

A5 is configured as:

```cpp
pinMode(A5, INPUT);
```

No internal pull-up is used.

---

# 5. External Button Functions

## Button A

### Short Press

A short press previews the next flight mode.

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

The mode is only **previewed**.

It is not applied to the flight controller until Button B is pressed.

### Hold 1.5 Seconds

Holding Button A for approximately **1.5 seconds** toggles:

```text
ARM
```

or

```text
DISARM
```

The firmware prevents a long ARM/DISARM hold from also being interpreted as a short mode-change press.

---

# 6. Button B

A short press of Button B applies the currently previewed mode.

Example:

```text
Current Mode:
STABILIZE

Press A:
Preview HOLD ALT

Press B:
Apply HOLD ALT
```

The selected mode is stored in:

```cpp
currentMode
```

while the previewed mode is stored in:

```cpp
pendingMode
```

---

# 7. LCD Keypad Controls

The 1602 LCD keypad provides an additional control interface.

| Button | Function               |
| ------ | ---------------------- |
| RIGHT  | Preview next mode      |
| LEFT   | ARM / DISARM           |
| UP     | RTH                    |
| DOWN   | Emergency              |
| SELECT | Cancel RTH / Emergency |

### RIGHT

Cycles through:

```text
STABILIZE
HOLD ALT
FREE
SAFE LAND
```

### LEFT

Calls the same ARM/DISARM safety routine used by Button A.

### UP

Activates:

```text
RTH = ON
```

and clears emergency mode.

### DOWN

Activates emergency mode and forces:

```text
ARM = OFF
```

### SELECT

Cancels:

```text
RTH
```

and:

```text
EMERGENCY
```

---

# 8. Flight Modes

The controller supports four modes.

| ID | Mode                |
| -: | ------------------- |
|  0 | STARTUP / STABILIZE |
|  1 | HOLD ALT            |
|  2 | FREE                |
|  3 | SAFE LAND           |

Defined in the firmware as:

```cpp
#define MODE_STABILIZE 0
#define MODE_HOLD_ALT  1
#define MODE_FREE      2
#define MODE_SAFE_LAND 3
```

---

# 9. ARM Safety Logic

The transmitter does not immediately ARM without checking the safety conditions.

ARM is blocked when:

* Throttle calibration has not completed
* Throttle is above the ARM limit
* RTH is active
* Emergency mode is active

The throttle must be below:

```cpp
throttleArmLimit = 35;
```

before ARM is accepted.

The controller also continuously receives FC telemetry and treats the flight controller as authoritative for the actual ARM state.

---

# 10. Throttle Calibration

At startup the transmitter performs automatic throttle calibration.

The display shows:

```text
THR CALIBRATE
KEEP DOWN...
```

The firmware samples the throttle input:

```cpp
const int samples = 100;
```

and calculates the minimum throttle ADC value.

The resulting value is stored in:

```cpp
throttleRawLow
```

The display then shows:

```text
THR CALIBRATED
LOW=xxxx
```

### Important

Keep the throttle joystick at its minimum position during startup calibration.

---

# 11. Joystick Processing

Joystick ADC values are converted to control values.

### Throttle

```text
0 → 1000
```

### Yaw

```text
-1000 → +1000
```

### Roll

```text
-1000 → +1000
```

### Pitch

```text
-1000 → +1000
```

Roll, pitch, and yaw have a deadzone of approximately:

```cpp
35
```

This prevents small joystick noise from generating unwanted commands.

---

# 12. Flight Controller Control Packet

The transmitter sends the following packet:

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

### Packet Fields

| Field     | Description                  |
| --------- | ---------------------------- |
| throttle  | 0–1000 throttle command      |
| roll      | Approximately -100 to +100   |
| pitch     | Approximately -100 to +100   |
| yaw       | Approximately -100 to +100   |
| mode      | Selected flight mode         |
| arm       | ARM/DISARM state             |
| rth       | RTH command                  |
| emergency | Emergency command            |
| sequence  | Incrementing packet sequence |

The packet is transmitted approximately every:

```text
50 ms
```

or:

```text
20 packets/second
```

---

# 13. Telemetry Packet

The remote receives telemetry from the flight controller.

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

### Telemetry Fields

| Field       | Description                   |
| ----------- | ----------------------------- |
| mode        | Current FC flight mode        |
| armed       | Actual FC ARM state           |
| link        | Link status                   |
| gpsFix      | GPS fix status                |
| sats        | Satellite count               |
| altitude_dm | Altitude in decimeters        |
| distance_m  | Distance in meters            |
| battery_cV  | Battery voltage in centivolts |
| failsafe    | FC failsafe status            |
| rth         | FC RTH status                 |

---

# 14. Telemetry Link Monitoring

The transmitter considers the telemetry link valid when telemetry has been received within:

```cpp
#define TELEMETRY_TIMEOUT 1500UL
```

Therefore:

```text
< 1.5 seconds = LINK OK
≥ 1.5 seconds = WAITING
```

The FC telemetry is also used to synchronize the transmitter's ARM state.

---

# 15. nRF24L01 Configuration

Both the remote and flight controller must use matching radio parameters.

```cpp
Channel:     108
Data Rate:   250 KBPS
CRC:         16-bit
PA Level:    LOW
Auto ACK:    Enabled
```

### Address Configuration

Control pipe:

```cpp
0xE8E8F0F0E1LL
```

Telemetry pipe:

```cpp
0xE8E8F0F0E2LL
```

The flight controller must use the corresponding opposite direction for its writing/reading pipes.

---

# 16. Radio Communication

The remote operates as a bidirectional radio controller.

### Transmitter → Flight Controller

```text
ControlPacket
     |
     v
nRF24L01
     |
     v
Flight Controller
```

### Flight Controller → Transmitter

```text
TelemetryPacket
     |
     v
nRF24L01
     |
     v
Remote LCD
```

The remote switches the nRF24 between transmit and receive operation:

```cpp
radio.stopListening();

radio.write(...);

radio.startListening();
```

---

# 17. LCD Startup Sequence

The LCD starts with:

```text
WELCOME TO
JIHAZ369
```

The startup interface then progresses through:

### 1. Welcome

```text
WELCOME TO
JIHAZ369
```

### 2. Radio Status

```text
NRF24 RADIO
CH108 250KBPS
```

or:

```text
NRF24 RADIO
RADIO ERROR
```

### 3. FC Telemetry

```text
FC TELEMETRY
LINK OK
```

or:

```text
FC TELEMETRY
WAITING...
```

### 4. Joystick Display

Example:

```text
T:0 Y:0
R:0 P:0 D
```

`A` indicates armed.

`D` indicates disarmed.

---

# 18. LCD Joystick Display

The normal display shows:

```text
T:000 Y:000
R:000 P:000 D
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

The joystick display refreshes approximately every:

```text
150 ms
```

---

# 19. Mode Preview Display

When a mode is previewed:

```text
PREVIEW MODE:
HOLD ALT
```

The preview remains separate from the currently selected mode.

This prevents an accidental short press of Button A from immediately changing the flight mode.

---

# 20. Mode Selection Workflow

The intended external-button workflow is:

```text
             BUTTON A
                 |
                 v
          Preview next mode
                 |
                 v
          PREVIEW MODE:
             HOLD ALT
                 |
                 v
             BUTTON B
                 |
                 v
          MODE SELECTED
```

This provides a two-step mode-selection mechanism.

---

# 21. ARM Workflow

### Button A

Hold for:

```text
1.5 seconds
```

The controller checks:

```text
Throttle calibrated?
        |
        +-- NO --> ARM BLOCKED
        |
       YES
        |
Throttle <= 35?
        |
        +-- NO --> ARM BLOCKED
        |
       YES
        |
RTH active?
        |
        +-- YES --> ARM BLOCKED
        |
       NO
        |
Emergency active?
        |
        +-- YES --> ARM BLOCKED
        |
       NO
        |
        v
      ARM ON
```

---

# 22. Emergency Workflow

Press:

```text
DOWN
```

The controller:

```text
Emergency = ON
RTH = OFF
ARM = OFF
```

LCD:

```text
EMERGENCY
ARM = OFF
```

Emergency can be cleared using:

```text
SELECT
```

---

# 23. RTH Workflow

Press:

```text
UP
```

The controller sends:

```text
RTH = ON
```

and disables the emergency command.

LCD:

```text
RTH COMMAND
RTH = ON
```

RTH can be cancelled using:

```text
SELECT
```

---

# 24. Software Requirements

Install the following Arduino libraries:

```text
SPI
nRF24L01
RF24
LiquidCrystal
```

The standard Arduino libraries:

```cpp
#include <SPI.h>
#include <LiquidCrystal.h>
```

The RF24 library provides:

```cpp
#include <nRF24L01.h>
#include <RF24.h>
```

---

# 25. Arduino IDE

Recommended:

```text
Arduino IDE
```

Board:

```text
Arduino UNO
```

Processor:

```text
ATmega328P
```

Select the correct serial/USB port before uploading.

---

# 26. Installation

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/JIHAZ369-QUADCOPTER.git
```

Open the remote-controller sketch in Arduino IDE.

Verify:

```text
Board: Arduino UNO
Processor: ATmega328P
Port: Correct COM port
```

Install the RF24 library if it is not already installed.

Compile the sketch.

Upload it to the Arduino UNO.

---

# 27. First Power-Up Test

Perform the first test **without propellers**.

Recommended sequence:

```text
1. Power the UNO
2. Keep throttle at minimum
3. Wait for throttle calibration
4. Verify LCD startup
5. Verify nRF24 status
6. Verify FC telemetry
7. Test joystick readings
8. Test Button A short press
9. Test Button B mode selection
10. Test Button A 1.5 s ARM hold
11. Test Button A 1.5 s DISARM hold
12. Test RTH
13. Test emergency
14. Test cancellation
```

---

# 28. A5 Button Troubleshooting

If Button A is not detected:

Check the ADC value using a temporary test sketch.

Expected:

```text
Released ≈ 1023
Button A ≈ 0
Button B ≈ 327
```

If Button B does not read around 327, check:

* 10 kΩ resistor
* 4.7 kΩ resistor
* Button wiring
* A5 connection
* GND connection
* 5 V supply
* resistor values

The firmware currently uses:

```cpp
AB_A_MAX       = 100
AB_B_MIN       = 200
AB_B_MAX       = 500
AB_RELEASE_MIN = 700
```

---

# 29. nRF24 Troubleshooting

If the LCD displays:

```text
RADIO ERROR
```

check:

* CE → D2
* CSN → D3
* MOSI → D11
* MISO → D12
* SCK → D13
* VCC → 3.3 V
* GND → GND

Also verify that the nRF24 has a stable power supply.

For difficult nRF24 modules, adding a capacitor close to the module's VCC/GND pins can improve power stability.

Both transmitter and flight controller must match:

```text
Channel
Data rate
CRC
Pipe addresses
Packet structure
```

---

# 30. Packet Compatibility

The `ControlPacket` and `TelemetryPacket` structures must remain compatible with the flight-controller firmware.

Do not independently change:

```cpp
uint16_t
int16_t
uint8_t
```

types or field ordering without updating the flight controller.

The packet layout is part of the communication protocol.

---

# 31. Project Architecture

```text
                    JIHAZ369 REMOTE
                         |
       +-----------------+------------------+
       |                 |                  |
       v                 v                  v
   Joysticks          A/B Buttons       LCD Keypad
       |                 |                  |
       +-----------------+------------------+
                         |
                         v
                 Control State
                         |
                         v
                  ControlPacket
                         |
                         v
                    nRF24L01
                         |
                 2.4 GHz Radio
                         |
                         v
              JIHAZ369 FLIGHT
                 CONTROLLER
                         |
                         v
                  TelemetryPacket
                         |
                         v
                    nRF24L01
                         |
                         v
                  Remote Controller
                         |
                         v
                       LCD
```

---

# 32. Main Firmware Functions

### Input

```cpp
readJoysticks()
readABRaw()
readKey()
```

### Button processing

```cpp
processExternalButtons()
processKeypad()
```

### ARM control

```cpp
toggleArm()
```

### Radio

```cpp
setupRadio()
sendControlPacket()
receiveTelemetry()
```

### Packet generation

```cpp
buildControlPacket()
```

### LCD

```cpp
showWelcome()
showRadioStatus()
showTelemetry()
showJoystickData()
showMode()
showMessage()
updateLCD()
```

### Calibration

```cpp
calibrateThrottle()
```

---

# 33. Timing

| Function              | Interval |
| --------------------- | -------: |
| Control transmission  |    50 ms |
| Control frequency     |    20 Hz |
| Joystick LCD refresh  |   150 ms |
| A/B debounce          |    35 ms |
| ARM hold              |  1500 ms |
| Telemetry timeout     |  1500 ms |
| Mode display          |  3000 ms |
| Radio startup display |  4000 ms |
| Welcome display       |  5000 ms |

---

# 34. Current Version

```text
Project: JIHAZ369 QUADCOPTER REMOTE CONTROLLER
Version: V1.8
Platform: Arduino UNO
Radio: nRF24L01
Display: 1602 LCD Keypad Shield
Button System: A5 Analog Resistor Ladder
```

---

# 35. Version History

## V1.8

### Added

* A5 shared A/B button system
* Analog resistor-ladder detection
* Button A short-press mode preview
* Button A 1.5-second ARM/DISARM hold
* Button B mode selection
* Long-hold protection against accidental mode changes
* Automatic throttle calibration
* FC telemetry synchronization
* LCD mode-preview interface

### Safety improvements

* ARM blocked when throttle is high
* ARM blocked before throttle calibration
* ARM blocked during RTH
* ARM blocked during emergency
* Emergency forces ARM OFF
* RTH and emergency states are mutually exclusive

---

# 36. Repository Structure

Recommended repository layout:

```text
JIHAZ369-QUADCOPTER/
│
├── README.md
│
├── remote/
│   └── JIHAZ369_Remote_V1.8.ino
│
├── flight-controller/
│   └── JIHAZ369_Flight_Controller.ino
│
├── processing/
│   └── JIHAZ369_Processing_Dashboard/
│
├── docs/
│   ├── wiring.md
│   ├── radio-protocol.md
│   └── testing.md
│
└── LICENSE
```

---

# 37. Development Status

```text
[✓] Arduino UNO remote controller
[✓] LCD interface
[✓] Four-axis joystick input
[✓] Throttle calibration
[✓] nRF24L01 communication
[✓] A5 A/B button ladder
[✓] ARM/DISARM logic
[✓] Mode preview
[✓] Mode selection
[✓] RTH command
[✓] Emergency command
[✓] Telemetry reception
[✓] Telemetry timeout detection
[ ] Full flight testing
[ ] Propeller-on testing
[ ] Outdoor flight validation
```

---

# 38. Important Testing Rule

The software should first be tested with:

```text
NO PROPELLERS
```

Recommended development sequence:

```text
USB Serial / LCD
       ↓
Joystick testing
       ↓
Button testing
       ↓
Radio testing
       ↓
Telemetry testing
       ↓
FC ARM testing
       ↓
ESC testing
       ↓
Motor testing
       ↓
Controlled flight testing
```

Each stage should be validated before proceeding to the next stage.

---

# 39. Credits

**JIHAZ369**

JIHAZ369 is an independent electronics, embedded systems, robotics, RF, AI, and flight-control development project.

Website:

**https://jihaz369.com**

---

# 40. License

This project is released under the **MIT License**.

```text
MIT License

Copyright (c) 2026 JIHAZ369

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions
:

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

## JIHAZ369

**JIHAZ369 QUADCOPTER SYSTEM**

```text
BUILD
TEST
LEARN
IMPROVE
```

**Version:** V1.8
**Platform:** Arduino UNO
**Radio:** nRF24L01
**Controller:** JIHAZ369

## JIHAZ369

```text
BUILD
TEST
LEARN
IMPROVE

JIHAZ369 QUADCOPTER SYSTEM
V1.8
```
