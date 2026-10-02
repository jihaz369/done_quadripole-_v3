/*
  ============================================================
  JIHAZ369 QUADCOPTER REMOTE CONTROLLER
  V1.8 - A5 SHARED A/B BUTTON SYSTEM
  ============================================================

  BOARD:
    Arduino UNO

  LCD 1602 KEYPAD SHIELD:
    RS = D8
    EN = D9
    D4 = D4
    D5 = D5
    D6 = D6
    D7 = D7
    KEYPAD = A0

  nRF24L01:
    CE  = D2
    CSN = D3
    MOSI = D11
    MISO = D12
    SCK  = D13

  JOYSTICKS:
    YAW      = A1
    THROTTLE = A2
    ROLL     = A3
    PITCH    = A4

  SHARED A/B BUTTON INPUT:
    A5

  BUTTON LADDER:

                 +5V
                  |
                 10k
                  |
                  +------ A5
                  |
              BUTTON A
                  |
                 GND

    A5 -------- 4.7k -------- BUTTON B -------- GND

  A5 ADC:
    Released = approximately 1023
    Button A = approximately 0
    Button B = approximately 327

  BUTTON A:
    Short press      = preview next mode
    Hold 1.5 seconds = ARM / DISARM

  IMPORTANT:
    A long hold must NOT also change the mode.

  BUTTON B:
    Short press = select/apply previewed mode

  LCD KEYPAD:
    RIGHT  = next mode
    LEFT   = ARM / DISARM
    UP     = RTH
    DOWN   = EMERGENCY
    SELECT = CANCEL RTH / EMERGENCY

  RADIO:
    Channel = 108
    Speed   = 250KBPS
    CRC     = 16 bit
    PA      = LOW

  PIPES:
    CONTROL = 0xE8E8F0F0E1LL
    TELEMETRY = 0xE8E8F0F0E2LL

  BENCH TEST ONLY - REMOVE PROPELLERS
  ============================================================
*/

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LiquidCrystal.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

// nRF24
#define NRF_CE   2
#define NRF_CSN  3

// LCD
#define LCD_RS   8
#define LCD_EN   9
#define LCD_D4   4
#define LCD_D5   5
#define LCD_D6   6
#define LCD_D7   7

// Joysticks
#define JOY_YAW       A1
#define JOY_THROTTLE  A2
#define JOY_ROLL      A3
#define JOY_PITCH     A4

// Shared A/B button ladder
#define BUTTON_AB_PIN A5

// ============================================================
// BUTTON A/B ADC LIMITS
// ============================================================

#define AB_A_MAX       100
#define AB_B_MIN       200
#define AB_B_MAX       500
#define AB_RELEASE_MIN 700

#define AB_DEBOUNCE_MS 35
#define ARM_HOLD_TIME  1500UL

// ============================================================
// RADIO
// ============================================================

RF24 radio(NRF_CE, NRF_CSN);

const uint64_t PIPE_CONTROL =
  0xE8E8F0F0E1LL;

const uint64_t PIPE_TELEM =
  0xE8E8F0F0E2LL;

#define NRF_CHANNEL 108

// ============================================================
// LCD
// ============================================================

LiquidCrystal lcd(
  LCD_RS,
  LCD_EN,
  LCD_D4,
  LCD_D5,
  LCD_D6,
  LCD_D7
);

// ============================================================
// MODES
// ============================================================

#define MODE_STABILIZE 0
#define MODE_HOLD_ALT   1
#define MODE_FREE       2
#define MODE_SAFE_LAND  3

// ============================================================
// PACKETS
// MUST MATCH FLIGHT CONTROLLER
// ============================================================

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

// ============================================================
// PACKET VARIABLES
// ============================================================

ControlPacket txPacket;
TelemetryPacket telem;

// ============================================================
// RADIO STATE
// ============================================================

bool radioOK = false;

unsigned long lastSendTime = 0;

#define SEND_INTERVAL 50UL

uint8_t sequenceNumber = 0;

unsigned long lastTelemetryTime = 0;

#define TELEMETRY_TIMEOUT 1500UL

// ============================================================
// CONTROL STATE
// ============================================================

bool armed = false;

bool rthActive = false;

bool emergencyActive = false;

uint8_t currentMode = MODE_STABILIZE;

uint8_t pendingMode = MODE_STABILIZE;

// ============================================================
// THROTTLE CALIBRATION
// ============================================================

bool throttleCalibrated = false;

int throttleRawLow = 0;

int throttleArmLimit = 35;

// ============================================================
// JOYSTICK VALUES
// ============================================================

int throttleValue = 0;

int yawValue = 0;

int rollValue = 0;

int pitchValue = 0;

// ============================================================
// A/B BUTTON STATE
// ============================================================

enum ABState
{
  AB_NONE,
  AB_BUTTON_A,
  AB_BUTTON_B
};

ABState abRawState = AB_NONE;

ABState abStableState = AB_NONE;

ABState abLastRawState = AB_NONE;

unsigned long abLastChange = 0;

unsigned long buttonAHoldStart = 0;

bool buttonAHoldActive = false;

bool buttonAArmTriggered = false;

// ============================================================
// LCD STATES
// ============================================================

enum LCDState
{
  LCD_STARTUP_WELCOME,
  LCD_RADIO_STATUS,
  LCD_TELEMETRY,
  LCD_JOYSTICK,
  LCD_MODE
};

LCDState lcdState =
  LCD_STARTUP_WELCOME;

unsigned long lcdStateStart = 0;

unsigned long lastJoystickDisplay = 0;

#define WELCOME_TIME          5000UL
#define RADIO_STATUS_TIME     4000UL
#define TELEMETRY_TIME        2500UL
#define JOYSTICK_REFRESH_TIME 150UL
#define MODE_DISPLAY_TIME     3000UL

// ============================================================
// MESSAGE SYSTEM
// ============================================================

bool messageActive = false;

unsigned long messageStart = 0;

unsigned long messageDuration = 0;

// ============================================================
// KEYPAD
// ============================================================

enum Key
{
  KEY_NONE,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_SELECT
};

// ============================================================
// READ A/B ADC
// ============================================================

ABState readABRaw()
{
  int value = analogRead(BUTTON_AB_PIN);

  // Button A
  if (value <= AB_A_MAX)
  {
    return AB_BUTTON_A;
  }

  // Button B
  if (value >= AB_B_MIN &&
      value <= AB_B_MAX)
  {
    return AB_BUTTON_B;
  }

  // Released
  if (value >= AB_RELEASE_MIN)
  {
    return AB_NONE;
  }

  // Unknown middle area
  return AB_NONE;
}

// ============================================================
// PROCESS A/B DEBOUNCE
// ============================================================

void processExternalButtons()
{
  ABState raw = readABRaw();

  if (raw != abLastRawState)
  {
    abLastRawState = raw;

    abLastChange = millis();
  }

  if ((millis() - abLastChange) >= AB_DEBOUNCE_MS)
  {
    if (raw != abStableState)
    {
      ABState previous = abStableState;

      abStableState = raw;

      // ----------------------------------------------
      // BUTTON A PRESSED
      // ----------------------------------------------

      if (abStableState == AB_BUTTON_A)
      {
        buttonAHoldActive = true;

        buttonAArmTriggered = false;

        buttonAHoldStart = millis();
      }

      // ----------------------------------------------
      // BUTTON A HELD
      // ----------------------------------------------

      // Handled below continuously.

      // ----------------------------------------------
      // BUTTON A RELEASED
      // ----------------------------------------------

      if (previous == AB_BUTTON_A &&
          abStableState != AB_BUTTON_A)
      {
        if (!buttonAArmTriggered)
        {
          // SHORT PRESS = NEXT MODE

          pendingMode++;

          if (pendingMode > MODE_SAFE_LAND)
          {
            pendingMode = MODE_STABILIZE;
          }

          showMode();
        }

        buttonAHoldActive = false;

        buttonAArmTriggered = false;
      }

      // ----------------------------------------------
      // BUTTON B PRESS
      // ----------------------------------------------

      if (abStableState == AB_BUTTON_B &&
          previous != AB_BUTTON_B)
      {
        currentMode = pendingMode;

        showMessage(
          "MODE SELECTED",
          armed ? "ARM:ON" : "ARM:OFF",
          1200
        );
      }
    }
  }

  // ==========================================================
  // A HOLD
  // ==========================================================

  if (abStableState == AB_BUTTON_A &&
      buttonAHoldActive)
  {
    unsigned long held =
      millis() - buttonAHoldStart;

    if (!buttonAArmTriggered &&
        held >= ARM_HOLD_TIME)
    {
      toggleArm();

      buttonAArmTriggered = true;
    }
  }
}

// ============================================================
// TOGGLE ARM
// ============================================================

void toggleArm()
{
  if (armed)
  {
    armed = false;

    rthActive = false;
    emergencyActive = false;

    showMessage(
      "ARM = OFF",
      "MOTORS SAFE",
      1200
    );

    return;
  }

  // ----------------------------------------------------------
  // ARM SAFETY
  // ----------------------------------------------------------

  if (!throttleCalibrated)
  {
    showMessage(
      "THR NOT READY",
      "CALIBRATION",
      1500
    );

    return;
  }

  if (throttleValue > throttleArmLimit)
  {
    showMessage(
      "LOWER THROTTLE",
      "ARM BLOCKED",
      1500
    );

    return;
  }

  if (rthActive)
  {
    showMessage(
      "RTH ACTIVE",
      "ARM BLOCKED",
      1500
    );

    return;
  }

  if (emergencyActive)
  {
    showMessage(
      "EMERGENCY ON",
      "ARM BLOCKED",
      1500
    );

    return;
  }

  // ----------------------------------------------------------
  // ARM
  // ----------------------------------------------------------

  armed = true;

  showMessage(
    "ARM = ON",
    "THR SAFE",
    1200
  );
}

// ============================================================
// MESSAGE
// ============================================================

void showMessage(
  const char *line1,
  const char *line2,
  unsigned long duration
)
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print(line1);

  lcd.setCursor(0, 1);

  lcd.print(line2);

  messageActive = true;

  messageStart = millis();

  messageDuration = duration;
}

// ============================================================
// MODE NAME
// ============================================================

const char *modeName(uint8_t mode)
{
  switch (mode)
  {
    case MODE_STABILIZE:
      return "STABILIZE";

    case MODE_HOLD_ALT:
      return "HOLD ALT";

    case MODE_FREE:
      return "FREE";

    case MODE_SAFE_LAND:
      return "SAFE LAND";
  }

  return "UNKNOWN";
}

// ============================================================
// SHOW MODE
// ============================================================

void showMode()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("PREVIEW MODE:");

  lcd.setCursor(0, 1);

  lcd.print(modeName(pendingMode));

  lcdState = LCD_MODE;

  lcdStateStart = millis();
}

// ============================================================
// READ JOYSTICKS
// ============================================================

void readJoysticks()
{
  int rawThrottle =
    analogRead(JOY_THROTTLE);

  // ----------------------------------------------------------
  // THROTTLE
  // ----------------------------------------------------------

  rawThrottle =
    constrain(
      rawThrottle,
      throttleRawLow,
      1023
    );

  if (1023 > throttleRawLow)
  {
    throttleValue =
      map(
        rawThrottle,
        throttleRawLow,
        1023,
        0,
        1000
      );
  }
  else
  {
    throttleValue = 0;
  }

  throttleValue =
    constrain(
      throttleValue,
      0,
      1000
    );

  // ----------------------------------------------------------
  // YAW
  // ----------------------------------------------------------

  int rawYaw =
    analogRead(JOY_YAW);

  yawValue =
    map(
      rawYaw,
      0,
      1023,
      -1000,
      1000
    );

  // ----------------------------------------------------------
  // ROLL
  // ----------------------------------------------------------

  int rawRoll =
    analogRead(JOY_ROLL);

  rollValue =
    map(
      rawRoll,
      0,
      1023,
      -1000,
      1000
    );

  // ----------------------------------------------------------
  // PITCH
  // ----------------------------------------------------------

  int rawPitch =
    analogRead(JOY_PITCH);

  pitchValue =
    map(
      rawPitch,
      0,
      1023,
      -1000,
      1000
    );

  // ----------------------------------------------------------
  // DEADZONE
  // ----------------------------------------------------------

  if (abs(yawValue) < 35)
    yawValue = 0;

  if (abs(rollValue) < 35)
    rollValue = 0;

  if (abs(pitchValue) < 35)
    pitchValue = 0;
}

// ============================================================
// THROTTLE CALIBRATION
// ============================================================

void calibrateThrottle()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("THR CALIBRATE");

  lcd.setCursor(0, 1);

  lcd.print("KEEP DOWN...");

  delay(500);

  long total = 0;

  const int samples = 100;

  for (int i = 0; i < samples; i++)
  {
    total += analogRead(JOY_THROTTLE);

    delay(5);
  }

  throttleRawLow =
    total / samples;

  throttleArmLimit = 35;

  throttleCalibrated = true;

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("THR CALIBRATED");

  lcd.setCursor(0, 1);

  lcd.print("LOW=");

  lcd.print(throttleRawLow);

  delay(1000);
}

// ============================================================
// KEYPAD READ
// ============================================================

Key readKey()
{
  int value =
    analogRead(A0);

  // Typical 1602 LCD keypad shield
  if (value < 60)
    return KEY_RIGHT;

  if (value < 200)
    return KEY_UP;

  if (value < 400)
    return KEY_DOWN;

  if (value < 600)
    return KEY_LEFT;

  if (value < 800)
    return KEY_SELECT;

  return KEY_NONE;
}

// ============================================================
// PROCESS KEYPAD
// ============================================================

void processKeypad()
{
  static Key lastKey = KEY_NONE;

  Key key = readKey();

  if (key != KEY_NONE &&
      lastKey == KEY_NONE)
  {
    switch (key)
    {
      // ------------------------------------------------------
      // RIGHT = NEXT MODE
      // ------------------------------------------------------

      case KEY_RIGHT:

        pendingMode++;

        if (pendingMode > MODE_SAFE_LAND)
        {
          pendingMode = MODE_STABILIZE;
        }

        showMode();

        break;

      // ------------------------------------------------------
      // UP = RTH
      // ------------------------------------------------------

      case KEY_UP:

        rthActive = true;

        emergencyActive = false;

        showMessage(
          "RTH COMMAND",
          "RTH = ON",
          1200
        );

        break;

      // ------------------------------------------------------
      // DOWN = EMERGENCY
      // ------------------------------------------------------

      case KEY_DOWN:

        emergencyActive = true;

        rthActive = false;

        armed = false;

        showMessage(
          "EMERGENCY",
          "ARM = OFF",
          1500
        );

        break;

      // ------------------------------------------------------
      // LEFT = ARM / DISARM
      // ------------------------------------------------------

      case KEY_LEFT:

        toggleArm();

        break;

      // ------------------------------------------------------
      // SELECT = CANCEL
      // ------------------------------------------------------

      case KEY_SELECT:

        rthActive = false;

        emergencyActive = false;

        showMessage(
          "COMMAND",
          "CANCELLED",
          1200
        );

        break;

      default:
        break;
    }
  }

  lastKey = key;
}

// ============================================================
// SHOW WELCOME
// ============================================================

void showWelcome()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("WELCOME TO");

  lcd.setCursor(0, 1);

  lcd.print("JIHAZ369");
}

// ============================================================
// SHOW RADIO STATUS
// ============================================================

void showRadioStatus()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("NRF24 RADIO");

  lcd.setCursor(0, 1);

  if (radioOK)
  {
    lcd.print("CH108 250KBPS");
  }
  else
  {
    lcd.print("RADIO ERROR");
  }
}

// ============================================================
// SHOW TELEMETRY
// ============================================================

void showTelemetry()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("FC TELEMETRY");

  lcd.setCursor(0, 1);

  if (telemetryLinkOK())
  {
    lcd.print("LINK OK");
  }
  else
  {
    lcd.print("WAITING...");
  }
}

// ============================================================
// SHOW JOYSTICK
// ============================================================

void showJoystickData()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("T:");

  lcd.print(throttleValue);

  lcd.print(" Y:");

  lcd.print(yawValue / 10);

  lcd.setCursor(0, 1);

  lcd.print("R:");

  lcd.print(rollValue / 10);

  lcd.print(" P:");

  lcd.print(pitchValue / 10);

  lcd.print(armed ? " A" : " D");
}

// ============================================================
// BUILD CONTROL PACKET
// ============================================================

void buildControlPacket()
{
  readJoysticks();

  txPacket.throttle =
    throttleValue;

  /*
    FC expects approximately -100..100
    for roll/pitch/yaw.
  */

  txPacket.roll =
    rollValue / 10;

  txPacket.pitch =
    pitchValue / 10;

  txPacket.yaw =
    yawValue / 10;

  txPacket.mode =
    currentMode;

  txPacket.arm =
    armed ? 1 : 0;

  txPacket.rth =
    rthActive ? 1 : 0;

  txPacket.emergency =
    emergencyActive ? 1 : 0;

  txPacket.sequence =
    sequenceNumber++;
}

// ============================================================
// SEND CONTROL PACKET
// ============================================================

void sendControlPacket()
{
  if (!radioOK)
    return;

  buildControlPacket();

  radio.stopListening();

  radio.write(
    &txPacket,
    sizeof(txPacket)
  );

  radio.startListening();
}

// ============================================================
// RECEIVE TELEMETRY
// ============================================================

void receiveTelemetry()
{
  if (!radioOK)
    return;

  while (radio.available())
  {
    radio.read(
      &telem,
      sizeof(telem)
    );

    lastTelemetryTime =
      millis();

    // FC is authoritative for ARM state.
    armed =
      telem.armed != 0;

    // Synchronize mode when not previewing
    if (pendingMode == currentMode)
    {
      currentMode =
        telem.mode;

      pendingMode =
        currentMode;
    }
  }
}

// ============================================================
// TELEMETRY LINK
// ============================================================

bool telemetryLinkOK()
{
  return
    (millis() - lastTelemetryTime)
    < TELEMETRY_TIMEOUT;
}

// ============================================================
// UPDATE LCD
// ============================================================

void updateLCD()
{
  unsigned long now =
    millis();

  // ----------------------------------------------------------
  // MESSAGE
  // ----------------------------------------------------------

  if (messageActive)
  {
    if (now - messageStart <
        messageDuration)
    {
      return;
    }

    messageActive = false;

    lcdState = LCD_JOYSTICK;

    lastJoystickDisplay = 0;

    readJoysticks();

    showJoystickData();

    return;
  }

  // ----------------------------------------------------------
  // STARTUP WELCOME
  // ----------------------------------------------------------

  if (lcdState ==
      LCD_STARTUP_WELCOME)
  {
    if (now - lcdStateStart <
        WELCOME_TIME)
    {
      return;
    }

    lcdState =
      LCD_RADIO_STATUS;

    lcdStateStart =
      now;

    showRadioStatus();

    return;
  }

  // ----------------------------------------------------------
  // RADIO
  // ----------------------------------------------------------

  if (lcdState ==
      LCD_RADIO_STATUS)
  {
    if (now - lcdStateStart <
        RADIO_STATUS_TIME)
    {
      return;
    }

    lcdState =
      LCD_TELEMETRY;

    lcdStateStart =
      now;

    showTelemetry();

    return;
  }

  // ----------------------------------------------------------
  // TELEMETRY
  // ----------------------------------------------------------

  if (lcdState ==
      LCD_TELEMETRY)
  {
    if (now - lcdStateStart <
        TELEMETRY_TIME)
    {
      return;
    }

    lcdState =
      LCD_JOYSTICK;

    lastJoystickDisplay = 0;

    readJoysticks();

    showJoystickData();

    return;
  }

  // ----------------------------------------------------------
  // JOYSTICK
  // ----------------------------------------------------------

  if (lcdState ==
      LCD_JOYSTICK)
  {
    if (now - lastJoystickDisplay >=
        JOYSTICK_REFRESH_TIME)
    {
      lastJoystickDisplay =
        now;

      readJoysticks();

      showJoystickData();
    }

    return;
  }

  // ----------------------------------------------------------
  // MODE
  // ----------------------------------------------------------

  if (lcdState ==
      LCD_MODE)
  {
    if (now - lcdStateStart >=
        MODE_DISPLAY_TIME)
    {
      lcdState =
        LCD_JOYSTICK;

      lastJoystickDisplay = 0;

      readJoysticks();

      showJoystickData();
    }

    return;
  }
}

// ============================================================
// RADIO INITIALIZATION
// ============================================================

void setupRadio()
{
  radioOK = false;

  if (!radio.begin())
  {
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("NRF24 ERROR");

    lcd.setCursor(0, 1);

    lcd.print("CHECK RADIO");

    return;
  }

  radio.setChannel(
    NRF_CHANNEL
  );

  radio.setDataRate(
    RF24_250KBPS
  );

  radio.setPALevel(
    RF24_PA_LOW
  );

  radio.setCRCLength(
    RF24_CRC_16
  );

  radio.setRetries(
    5,
    15
  );

  radio.setAutoAck(true);

  radio.openWritingPipe(
    PIPE_CONTROL
  );

  radio.openReadingPipe(
    1,
    PIPE_TELEM
  );

  radio.startListening();

  radioOK = true;
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  /*
    IMPORTANT:
    A5 is ANALOG ONLY for the shared button ladder.

    Do NOT use INPUT_PULLUP on A5.
  */

  pinMode(
    BUTTON_AB_PIN,
    INPUT
  );

  lcd.begin(
    16,
    2
  );

  memset(
    &txPacket,
    0,
    sizeof(txPacket)
  );

  memset(
    &telem,
    0,
    sizeof(telem)
  );

  currentMode =
    MODE_STABILIZE;

  pendingMode =
    MODE_STABILIZE;

  armed = false;

  rthActive = false;

  emergencyActive = false;

  sequenceNumber = 0;

  showWelcome();

  delay(5000);

  setupRadio();

  calibrateThrottle();

  showWelcome();

  lcdState =
    LCD_STARTUP_WELCOME;

  lcdStateStart =
    millis();

  lastJoystickDisplay = 0;
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now =
    millis();

  // A5 shared A/B buttons
  processExternalButtons();

  // LCD keypad
  processKeypad();

  // Receive FC telemetry
  receiveTelemetry();

  // Send control at 20 Hz
  if (now - lastSendTime >=
      SEND_INTERVAL)
  {
    lastSendTime = now;

    sendControlPacket();
  }

  // LCD
  updateLCD();
}