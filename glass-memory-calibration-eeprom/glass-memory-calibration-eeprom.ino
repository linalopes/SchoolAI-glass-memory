#include <MD_MAX72xx.h>
#include <SPI.h>
#include <EEPROM.h>

// ============================================================
// GLASS MEMORY CALIBRATION
// Board: Arduino Pro Micro / ATmega32U4
//
// Automatic sensor calibration using the MAX7219 display.
//
// Flow:
//   CAL
//   CLEAR -> countdown -> baseline
//   START -> countdown -> hand sample
//   UP    -> countdown -> hand sample
//   DOWN  -> countdown -> hand sample
//   LEFT  -> countdown -> hand sample
//   RIGHT -> countdown -> hand sample
//   DONE
//   TEST -> live sensor + relay validation
//
// Calibration is saved to EEPROM for the final game firmware.
//
// Display orientation is already physically fixed:
//   X axis reversed
//   Y axis normal
//
// Relay module:
//   ACTIVE LOW
//   IN1 = UP    -> D5
//   IN2 = DOWN  -> D4
//   IN3 = LEFT  -> D3
//   IN4 = RIGHT -> D2
//
// START does not activate a relay.
// ============================================================


// ------------------------------------------------------------
// MAX7219
// ------------------------------------------------------------

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define DATA_PIN 15
#define CS_PIN   14
#define CLK_PIN  16

MD_MAX72XX matrix(
  HARDWARE_TYPE,
  DATA_PIN,
  CLK_PIN,
  CS_PIN,
  MAX_DEVICES
);


// ------------------------------------------------------------
// RELAYS
// ------------------------------------------------------------

const uint8_t RELAY_UP_PIN    = 5;
const uint8_t RELAY_DOWN_PIN  = 4;
const uint8_t RELAY_LEFT_PIN  = 3;
const uint8_t RELAY_RIGHT_PIN = 2;

const uint8_t RELAY_ON  = LOW;
const uint8_t RELAY_OFF = HIGH;


// ------------------------------------------------------------
// ANALOG SENSOR PINOUT
// ------------------------------------------------------------

const uint8_t START_PIN = A10;
const uint8_t UP_PIN    = A9;
const uint8_t DOWN_PIN  = A8;
const uint8_t LEFT_PIN  = A0;
const uint8_t RIGHT_PIN = A7;


// ------------------------------------------------------------
// SENSOR INDEXES
// ------------------------------------------------------------

enum SensorId
{
  START_ID = 0,
  UP_ID,
  DOWN_ID,
  LEFT_ID,
  RIGHT_ID,
  SENSOR_COUNT
};

const uint8_t sensorPins[SENSOR_COUNT] =
{
  START_PIN,
  UP_PIN,
  DOWN_PIN,
  LEFT_PIN,
  RIGHT_PIN
};

const char* sensorNames[SENSOR_COUNT] =
{
  "START",
  "UP",
  "DOWN",
  "LEFT",
  "RIGHT"
};


// ------------------------------------------------------------
// CALIBRATION DATA
// ------------------------------------------------------------

int16_t baseline[SENSOR_COUNT];
int16_t baselineMin[SENSOR_COUNT];
int16_t baselineMax[SENSOR_COUNT];

int16_t handValue[SENSOR_COUNT];
int16_t handMin[SENSOR_COUNT];
int16_t handMax[SENSOR_COUNT];

int16_t deltaValue[SENSOR_COUNT];


// ------------------------------------------------------------
// EEPROM FORMAT
//
// Keep this structure identical in the final Glass Memory
// game firmware so it can load the saved calibration.
// ------------------------------------------------------------

const uint16_t EEPROM_MAGIC = 0x474D;  // "GM"
const uint8_t EEPROM_VERSION = 1;
const int EEPROM_ADDRESS = 0;

struct StoredCalibration
{
  uint16_t magic;
  uint8_t version;
  int16_t baseline[SENSOR_COUNT];
  int16_t hand[SENSOR_COUNT];
  uint16_t checksum;
};


// ------------------------------------------------------------
// LIVE SENSOR DATA
// ------------------------------------------------------------

int16_t rawValue[SENSOR_COUNT];
int16_t filteredValue[SENSOR_COUNT];
int16_t scorePercent[SENSOR_COUNT];

bool filtersInitialized = false;


// ------------------------------------------------------------
// DETECTION STATE
// ------------------------------------------------------------

int8_t activeSensor = -1;
int8_t pendingSensor = -2;

unsigned long pendingSince = 0;
unsigned long lastSerialPrint = 0;


// ------------------------------------------------------------
// SETTINGS
// ------------------------------------------------------------

// Calibration sampling
const uint16_t CALIBRATION_SAMPLES = 120;
const uint8_t CALIBRATION_DELAY_MS = 4;

// Time shown before each automatic measurement.
const uint16_t CLEAR_PREPARE_MS = 2200;
const uint16_t SENSOR_PREPARE_MS = 1800;
const uint16_t COUNTDOWN_STEP_MS = 700;
const uint16_t OK_DISPLAY_MS = 650;

// Each live reading averages several ADC conversions.
const uint8_t ADC_AVERAGE_SAMPLES = 4;

// Exponential smoothing:
// filtered = 75% old + 25% new
const uint8_t FILTER_OLD_WEIGHT = 3;
const uint8_t FILTER_TOTAL_WEIGHT = 4;

// Normalized activation:
// 0%   = baseline
// 100% = calibrated hand position
const int16_t TRIGGER_SCORE = 50;
const int16_t RELEASE_SCORE = 30;

// Winner must beat the second strongest sensor by this amount.
const int16_t WINNER_MARGIN = 10;

// New winner must remain stable for this long.
const unsigned long STABLE_TIME_MS = 80;

const unsigned long LIVE_PRINT_INTERVAL = 500;


// ============================================================
// 5x7 FONT
//
// Stored in flash. Each glyph is seven rows of five pixels.
// ============================================================

const uint8_t FONT_AZ[26][7] PROGMEM =
{
  {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
  {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // G
  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
  {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // J
  {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
  {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
  {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // N
  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, // S
  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // V
  {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, // W
  {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // Y
  {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}  // Z
};

const uint8_t FONT_DIGITS[10][7] PROGMEM =
{
  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
  {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, // 2
  {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}, // 3
  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
  {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}, // 5
  {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}, // 6
  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
  {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}  // 9
};


// ============================================================
// RELAY HELPERS
// ============================================================

void allRelaysOff()
{
  digitalWrite(RELAY_UP_PIN, RELAY_OFF);
  digitalWrite(RELAY_DOWN_PIN, RELAY_OFF);
  digitalWrite(RELAY_LEFT_PIN, RELAY_OFF);
  digitalWrite(RELAY_RIGHT_PIN, RELAY_OFF);
}


void updateRelaysForSensor(int8_t sensorId)
{
  allRelaysOff();

  switch (sensorId)
  {
    case UP_ID:
      digitalWrite(RELAY_UP_PIN, RELAY_ON);
      break;

    case DOWN_ID:
      digitalWrite(RELAY_DOWN_PIN, RELAY_ON);
      break;

    case LEFT_ID:
      digitalWrite(RELAY_LEFT_PIN, RELAY_ON);
      break;

    case RIGHT_ID:
      digitalWrite(RELAY_RIGHT_PIN, RELAY_ON);
      break;

    case START_ID:
    default:
      break;
  }
}


// ============================================================
// DISPLAY HELPERS
// ============================================================

void setPixel(uint8_t logicalRow, uint8_t logicalCol, bool state = true)
{
  // Physical installation is horizontally mirrored.
  uint8_t physicalRow = logicalRow;
  uint8_t physicalCol = 31 - logicalCol;

  matrix.setPoint(
    physicalRow,
    physicalCol,
    state
  );
}


void beginDisplayFrame()
{
  matrix.control(
    MD_MAX72XX::UPDATE,
    MD_MAX72XX::OFF
  );

  matrix.clear();
}


void endDisplayFrame()
{
  matrix.control(
    MD_MAX72XX::UPDATE,
    MD_MAX72XX::ON
  );
}


void clearDisplayAtomic()
{
  beginDisplayFrame();
  endDisplayFrame();
}


uint8_t getGlyphRow(char c, uint8_t row)
{
  if (c >= 'A' && c <= 'Z')
  {
    return pgm_read_byte(
      &FONT_AZ[c - 'A'][row]
    );
  }

  if (c >= '0' && c <= '9')
  {
    return pgm_read_byte(
      &FONT_DIGITS[c - '0'][row]
    );
  }

  return 0;
}


void drawCharacter(char c, int8_t x)
{
  for (uint8_t row = 0; row < 7; row++)
  {
    uint8_t bits = getGlyphRow(c, row);

    for (uint8_t col = 0; col < 5; col++)
    {
      if (bits & (1 << (4 - col)))
      {
        int8_t px = x + col;

        if (px >= 0 && px < 32)
        {
          setPixel(row, px);
        }
      }
    }
  }
}


uint8_t textWidth(const char* text)
{
  uint8_t length = strlen(text);

  if (length == 0)
  {
    return 0;
  }

  return (length * 6) - 1;
}


void showText(const char* text)
{
  beginDisplayFrame();

  uint8_t width = textWidth(text);
  int8_t x = (32 - width) / 2;

  for (uint8_t i = 0; text[i] != '\0'; i++)
  {
    drawCharacter(text[i], x);
    x += 6;
  }

  endDisplayFrame();
}


void showCountdown()
{
  showText("3");
  delay(COUNTDOWN_STEP_MS);

  showText("2");
  delay(COUNTDOWN_STEP_MS);

  showText("1");
  delay(COUNTDOWN_STEP_MS);
}


void showSensorMarker(uint8_t sensorId)
{
  beginDisplayFrame();

  switch (sensorId)
  {
    case START_ID:
      for (uint8_t r = 3; r <= 4; r++)
      {
        for (uint8_t c = 15; c <= 16; c++)
        {
          setPixel(r, c);
        }
      }
      break;

    case UP_ID:
      for (uint8_t c = 12; c <= 19; c++)
      {
        setPixel(0, c);
      }
      break;

    case DOWN_ID:
      for (uint8_t c = 12; c <= 19; c++)
      {
        setPixel(7, c);
      }
      break;

    case LEFT_ID:
      for (uint8_t r = 0; r < 8; r++)
      {
        setPixel(r, 0);
      }
      break;

    case RIGHT_ID:
      for (uint8_t r = 0; r < 8; r++)
      {
        setPixel(r, 31);
      }
      break;
  }

  endDisplayFrame();
}


// ============================================================
// ADC HELPERS
// ============================================================

int readAnalogAveraged(uint8_t pin)
{
  // Discard first conversion after channel switching.
  analogRead(pin);

  long total = 0;

  for (uint8_t i = 0; i < ADC_AVERAGE_SAMPLES; i++)
  {
    total += analogRead(pin);
  }

  return total / ADC_AVERAGE_SAMPLES;
}


// ============================================================
// CALIBRATION SAMPLING
// ============================================================

void sampleBaselineAll()
{
  long totals[SENSOR_COUNT] = { 0, 0, 0, 0, 0 };

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    baselineMin[i] = 1023;
    baselineMax[i] = 0;
  }

  for (uint16_t sample = 0;
       sample < CALIBRATION_SAMPLES;
       sample++)
  {
    for (uint8_t i = 0; i < SENSOR_COUNT; i++)
    {
      int value =
        readAnalogAveraged(sensorPins[i]);

      totals[i] += value;

      if (value < baselineMin[i])
      {
        baselineMin[i] = value;
      }

      if (value > baselineMax[i])
      {
        baselineMax[i] = value;
      }
    }

    delay(CALIBRATION_DELAY_MS);
  }

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    baseline[i] =
      totals[i] / CALIBRATION_SAMPLES;
  }
}


void sampleHand(uint8_t sensorId)
{
  long total = 0;

  handMin[sensorId] = 1023;
  handMax[sensorId] = 0;

  for (uint16_t sample = 0;
       sample < CALIBRATION_SAMPLES;
       sample++)
  {
    int value =
      readAnalogAveraged(sensorPins[sensorId]);

    total += value;

    if (value < handMin[sensorId])
    {
      handMin[sensorId] = value;
    }

    if (value > handMax[sensorId])
    {
      handMax[sensorId] = value;
    }

    delay(CALIBRATION_DELAY_MS);
  }

  handValue[sensorId] =
    total / CALIBRATION_SAMPLES;

  deltaValue[sensorId] =
    handValue[sensorId] -
    baseline[sensorId];
}


// ============================================================
// EEPROM
// ============================================================

uint16_t calculateChecksum(const StoredCalibration& data)
{
  uint16_t sum = data.magic + data.version;

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    sum += (uint16_t)data.baseline[i];
    sum += (uint16_t)data.hand[i];
  }

  return sum;
}


void saveCalibrationToEEPROM()
{
  StoredCalibration data;

  data.magic = EEPROM_MAGIC;
  data.version = EEPROM_VERSION;

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    data.baseline[i] = baseline[i];
    data.hand[i] = handValue[i];
  }

  data.checksum = calculateChecksum(data);

  EEPROM.put(
    EEPROM_ADDRESS,
    data
  );


  StoredCalibration verify;

  EEPROM.get(
    EEPROM_ADDRESS,
    verify
  );

  bool valid =
    verify.magic == EEPROM_MAGIC &&
    verify.version == EEPROM_VERSION &&
    verify.checksum == calculateChecksum(verify);

  if (valid)
  {
    Serial.println(F("EEPROM SAVE: OK"));
  }
  else
  {
    Serial.println(F("EEPROM SAVE: ERROR"));
  }
}


// ============================================================
// SERIAL OUTPUT
// ============================================================

void printOneCalibration(uint8_t i)
{
  int baselineNoise =
    baselineMax[i] - baselineMin[i];

  int handSpan =
    handMax[i] - handMin[i];

  Serial.print(sensorNames[i]);

  Serial.print(F("  base="));
  Serial.print(baseline[i]);

  Serial.print(F(" ["));
  Serial.print(baselineMin[i]);
  Serial.print(F(".."));
  Serial.print(baselineMax[i]);
  Serial.print(']');

  Serial.print(F("  hand="));
  Serial.print(handValue[i]);

  Serial.print(F(" ["));
  Serial.print(handMin[i]);
  Serial.print(F(".."));
  Serial.print(handMax[i]);
  Serial.print(']');

  Serial.print(F("  delta="));

  if (deltaValue[i] >= 0)
  {
    Serial.print('+');
  }

  Serial.print(deltaValue[i]);

  Serial.print(F("  noise="));
  Serial.print(baselineNoise);

  Serial.print(F("  handSpan="));
  Serial.println(handSpan);
}


void printCalibrationSummary()
{
  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("CALIBRATION SUMMARY"));
  Serial.println(F("================================"));

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    printOneCalibration(i);
  }


  Serial.println();
  Serial.println(F("C++ TABLE FOR REFERENCE"));
  Serial.println();

  Serial.print(F("const int16_t CAL_BASELINE[5] = { "));

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    Serial.print(baseline[i]);

    if (i < SENSOR_COUNT - 1)
    {
      Serial.print(F(", "));
    }
  }

  Serial.println(F(" };"));


  Serial.print(F("const int16_t CAL_HAND[5] = { "));

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    Serial.print(handValue[i]);

    if (i < SENSOR_COUNT - 1)
    {
      Serial.print(F(", "));
    }
  }

  Serial.println(F(" };"));

  Serial.println();
}


// ============================================================
// AUTOMATIC CALIBRATION
// ============================================================

void calibrateOneSensor(uint8_t sensorId)
{
  allRelaysOff();

  showText(sensorNames[sensorId]);

  Serial.println();
  Serial.print(F("Prepare hand over "));
  Serial.println(sensorNames[sensorId]);

  delay(SENSOR_PREPARE_MS);

  showCountdown();

  Serial.print(F("Sampling "));
  Serial.print(sensorNames[sensorId]);
  Serial.println(F("..."));

  sampleHand(sensorId);

  printOneCalibration(sensorId);

  showText("OK");
  delay(OK_DISPLAY_MS);
}


void runAutomaticCalibration()
{
  allRelaysOff();

  filtersInitialized = false;
  activeSensor = -1;
  pendingSensor = -2;

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("GLASS MEMORY CALIBRATION"));
  Serial.println(F("AUTOMATIC SENSOR CALIBRATION"));
  Serial.println(F("================================"));

  showText("CAL");
  delay(1200);


  // ----------------------------------------------------------
  // Baseline
  // ----------------------------------------------------------

  showText("CLEAR");

  Serial.println();
  Serial.println(F("Keep the complete sensor area clear."));

  delay(CLEAR_PREPARE_MS);

  showCountdown();

  Serial.println(F("Sampling baseline..."));

  sampleBaselineAll();

  Serial.println(F("Baseline captured."));

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    Serial.print(sensorNames[i]);
    Serial.print(F(" = "));
    Serial.println(baseline[i]);
  }


  // ----------------------------------------------------------
  // Hand samples
  // ----------------------------------------------------------

  calibrateOneSensor(START_ID);
  calibrateOneSensor(UP_ID);
  calibrateOneSensor(DOWN_ID);
  calibrateOneSensor(LEFT_ID);
  calibrateOneSensor(RIGHT_ID);


  // ----------------------------------------------------------
  // Save
  // ----------------------------------------------------------

  saveCalibrationToEEPROM();

  printCalibrationSummary();


  // ----------------------------------------------------------
  // Enter live validation
  // ----------------------------------------------------------

  filtersInitialized = false;
  activeSensor = -1;
  pendingSensor = -2;

  allRelaysOff();

  showText("DONE");
  delay(1500);

  showText("TEST");
  delay(900);

  clearDisplayAtomic();

  Serial.println(F("LIVE TEST"));
  Serial.println(F("Sensor calibration is stored in EEPROM."));
  Serial.println(F("Reset the board to run calibration again."));
  Serial.println(F("P = print calibration table again."));
  Serial.println();
}


// ============================================================
// NORMALIZED SENSOR SCORE
// ============================================================

int16_t calculateScorePercent(uint8_t sensorId, int value)
{
  int delta = deltaValue[sensorId];

  if (delta > -2 && delta < 2)
  {
    return 0;
  }

  long numerator =
    (long)(value - baseline[sensorId]) * 100L;

  long score =
    numerator / delta;

  if (score < -50)
  {
    score = -50;
  }

  if (score > 200)
  {
    score = 200;
  }

  return (int16_t)score;
}


// ============================================================
// LIVE FILTERING
// ============================================================

void readAllSensors()
{
  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    rawValue[i] =
      readAnalogAveraged(sensorPins[i]);

    if (!filtersInitialized)
    {
      filteredValue[i] = rawValue[i];
    }
    else
    {
      filteredValue[i] =
        (
          (long)filteredValue[i] *
          FILTER_OLD_WEIGHT
          +
          rawValue[i]
        )
        /
        FILTER_TOTAL_WEIGHT;
    }

    scorePercent[i] =
      calculateScorePercent(
        i,
        filteredValue[i]
      );
  }

  filtersInitialized = true;
}


// ============================================================
// WINNER SELECTION
// ============================================================

int8_t findValidWinner()
{
  int8_t bestSensor = -1;

  int16_t bestScore = -32768;
  int16_t secondScore = -32768;

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    int16_t score = scorePercent[i];

    if (score > bestScore)
    {
      secondScore = bestScore;
      bestScore = score;
      bestSensor = i;
    }
    else if (score > secondScore)
    {
      secondScore = score;
    }
  }

  if (bestScore < TRIGGER_SCORE)
  {
    return -1;
  }

  if (
    secondScore >= TRIGGER_SCORE
    &&
    (bestScore - secondScore) < WINNER_MARGIN
  )
  {
    return -1;
  }

  return bestSensor;
}


// ============================================================
// STABLE ACTIVE SENSOR STATE
// ============================================================

void updateActiveSensor()
{
  int8_t winner = findValidWinner();

  int8_t desiredSensor = activeSensor;

  if (activeSensor < 0)
  {
    desiredSensor = winner;
  }
  else
  {
    int16_t activeScore =
      scorePercent[activeSensor];

    if (activeScore < RELEASE_SCORE)
    {
      desiredSensor = winner;
    }
    else if (
      winner >= 0
      &&
      winner != activeSensor
      &&
      scorePercent[winner] >=
        activeScore + WINNER_MARGIN
    )
    {
      desiredSensor = winner;
    }
  }


  if (desiredSensor == activeSensor)
  {
    pendingSensor = -2;
    return;
  }


  if (pendingSensor != desiredSensor)
  {
    pendingSensor = desiredSensor;
    pendingSince = millis();
    return;
  }


  if (millis() - pendingSince < STABLE_TIME_MS)
  {
    return;
  }


  activeSensor = desiredSensor;
  pendingSensor = -2;


  updateRelaysForSensor(activeSensor);


  if (activeSensor >= 0)
  {
    showSensorMarker(activeSensor);
  }
  else
  {
    clearDisplayAtomic();
  }
}


// ============================================================
// LIVE SERIAL DIAGNOSTICS
// ============================================================

void printLiveData()
{
  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    Serial.print(sensorNames[i]);

    Serial.print(':');
    Serial.print(filteredValue[i]);

    Serial.print('/');
    Serial.print(scorePercent[i]);
    Serial.print('%');

    if ((int8_t)i == activeSensor)
    {
      Serial.print(F("[ACTIVE]"));
    }
    else if (scorePercent[i] >= TRIGGER_SCORE)
    {
      Serial.print(F("[CAND]"));
    }
    else
    {
      Serial.print(F("[--]"));
    }

    if (i < SENSOR_COUNT - 1)
    {
      Serial.print(F("  "));
    }
  }

  Serial.println();
}


// ============================================================
// OPTIONAL SERIAL COMMANDS
// ============================================================

void handleSerialCommand()
{
  if (!Serial.available())
  {
    return;
  }

  char command = Serial.read();

  while (Serial.available())
  {
    Serial.read();
  }

  if (command == 'P' || command == 'p')
  {
    printCalibrationSummary();
  }
}


// ============================================================
// LIVE TEST
// ============================================================

void runLiveTest()
{
  readAllSensors();

  updateActiveSensor();

  if (
    millis() - lastSerialPrint
    >= LIVE_PRINT_INTERVAL
  )
  {
    lastSerialPrint = millis();

    printLiveData();
  }

  handleSerialCommand();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  // Relay outputs
  pinMode(RELAY_UP_PIN, OUTPUT);
  pinMode(RELAY_DOWN_PIN, OUTPUT);
  pinMode(RELAY_LEFT_PIN, OUTPUT);
  pinMode(RELAY_RIGHT_PIN, OUTPUT);

  // Active LOW: HIGH means OFF.
  allRelaysOff();


  matrix.begin();

  matrix.control(
    MD_MAX72XX::INTENSITY,
    3
  );

  clearDisplayAtomic();


  // Give USB Serial a moment to attach, but calibration
  // does not depend on Serial being connected.
  unsigned long serialWaitStart = millis();

  while (
    !Serial
    &&
    millis() - serialWaitStart < 2000
  )
  {
  }


  Serial.println();
  Serial.println(F("GLASS MEMORY CALIBRATION"));
  Serial.println(F("Display-guided / EEPROM-enabled"));
  Serial.println();

  Serial.println(F("Sensor order:"));
  Serial.println(F("START A10"));
  Serial.println(F("UP    A9"));
  Serial.println(F("DOWN  A8"));
  Serial.println(F("LEFT  A0"));
  Serial.println(F("RIGHT A7"));
  Serial.println();

  runAutomaticCalibration();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  runLiveTest();
}
