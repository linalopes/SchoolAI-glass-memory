#include <MD_MAX72xx.h>
#include <SPI.h>
#include <EEPROM.h>

// ============================================================
// GLASS MEMORY - GAME V4
// Board: Arduino Pro Micro / ATmega32U4
//
// V4 is rebuilt around the validated PRESS -> LATCH ->
// RELEASE -> NEUTRAL -> ARMED input detector.
//
// Main changes from V1/V2/V3:
// - One sensor press is latched until THAT SAME sensor releases.
// - Repeated directions such as LEFT -> LEFT are independent events.
// - START only participates while idle.
// - Only UP/DOWN/LEFT/RIGHT participate during the player turn.
// - The analog filter stays continuous inside a player round.
// - The resting baseline slowly follows ambient optical drift in RAM.
// - EEPROM calibration is never rewritten by the game.
// - Player input timeout is per expected press.
// - Relay/lamp feedback stays on while the accepted hand is present.
//
// Display:
// - X axis reversed
// - Y axis normal
// - Display never reveals a direction.
//
// Relays:
// - ACTIVE LOW
// - IN1 = UP    -> D5
// - IN2 = DOWN  -> D4
// - IN3 = LEFT  -> D3
// - IN4 = RIGHT -> D2
// ============================================================


// ------------------------------------------------------------
// DEBUG
// ------------------------------------------------------------

#define DEBUG_SERIAL 1

#if DEBUG_SERIAL
  #define DBG_BEGIN(baud) Serial.begin(baud)
  #define DBG_PRINT(x) Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
#else
  #define DBG_BEGIN(baud)
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif


// ------------------------------------------------------------
// PLAYER LAMP FEEDBACK
//
// Set to 0 only for A/B diagnostics.
// Computer sequence playback always uses the lamps.
// ------------------------------------------------------------

#define PLAYER_LAMP_FEEDBACK 1


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


// Only sensors relevant to the current game state may compete.
const uint8_t START_SENSOR_MASK =
  (1 << START_ID);

const uint8_t DIRECTION_SENSOR_MASK =
  (1 << UP_ID) |
  (1 << DOWN_ID) |
  (1 << LEFT_ID) |
  (1 << RIGHT_ID);


// ------------------------------------------------------------
// EEPROM CALIBRATION FORMAT
//
// MUST remain identical to glass-memory-calibration-eeprom.ino
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
// ACTIVE CALIBRATION
//
// baseline[] is the live RAM baseline.
// EEPROM is never modified by the game.
// ------------------------------------------------------------

int16_t baseline[SENSOR_COUNT];
int16_t handValue[SENSOR_COUNT];
int16_t deltaValue[SENSOR_COUNT];

// Q8 fixed-point baseline for slow ambient tracking.
long baselineQ8[SENSOR_COUNT];


// ------------------------------------------------------------
// LIVE SENSOR DATA
// ------------------------------------------------------------

int16_t rawValue[SENSOR_COUNT];
int16_t filteredValue[SENSOR_COUNT];
int16_t scorePercent[SENSOR_COUNT];

bool filtersInitialized = false;


// ------------------------------------------------------------
// SENSOR SETTINGS
// ------------------------------------------------------------

const uint8_t ADC_AVERAGE_SAMPLES = 4;

// filtered = 75% previous + 25% new
const uint8_t FILTER_OLD_WEIGHT = 3;
const uint8_t FILTER_TOTAL_WEIGHT = 4;

// 0% = current resting baseline
// 100% = calibrated hand position
const int16_t TRIGGER_SCORE = 50;
const int16_t RELEASE_SCORE = 30;

const int16_t WINNER_MARGIN = 10;

const unsigned long PRESS_STABLE_MS   = 80;
const unsigned long RELEASE_STABLE_MS = 80;
const unsigned long REARM_NEUTRAL_MS  = 120;


// ------------------------------------------------------------
// SLOW AMBIENT BASELINE TRACKING
//
// The last tests showed resting values drifting to roughly
// 20-35% after calibration while real touches were ~95-102%.
//
// Tracking is deliberately slow and only occurs while a sensor
// is well below the press threshold. It changes RAM only.
// ------------------------------------------------------------

const int16_t BASELINE_TRACK_MAX_SCORE = 45;

const unsigned long BASELINE_TRACK_INTERVAL_MS = 100;

// Each tracking step moves 1/64 of the remaining error.
// At 100 ms per step this is intentionally slow.
const uint8_t BASELINE_TRACK_DIVISOR = 64;

// Never allow baseline tracking to collapse the useful span.
const int16_t MIN_LIVE_DELTA = 20;

unsigned long lastBaselineTrack = 0;


// ------------------------------------------------------------
// DISCRETE INPUT EVENT STATE
//
// This is the validated event machine.
// ------------------------------------------------------------

int8_t latchedSensor = -1;

int8_t candidateSensor = -1;
unsigned long candidateSince = 0;

unsigned long releaseSince = 0;
unsigned long neutralSince = 0;

bool eventArmed = false;

unsigned long inputEventCounter = 0;


// ------------------------------------------------------------
// GAME SETTINGS
// ------------------------------------------------------------

const uint8_t MAX_SEQUENCE_LENGTH = 32;

uint8_t sequence[MAX_SEQUENCE_LENGTH];
uint8_t sequenceLength = 0;
uint8_t completedRounds = 0;

const unsigned long START_COUNTDOWN_MS = 500;
const unsigned long LEVEL_DISPLAY_MS   = 600;

const unsigned long SEQUENCE_ON_MS  = 700;
const unsigned long SEQUENCE_GAP_MS = 250;

const unsigned long GO_DISPLAY_MS = 450;

// Time allowed to START a requested player press.
const unsigned long INPUT_TIMEOUT_MS = 10000;

// Once a press has been accepted, allow time for release.
const unsigned long RELEASE_TIMEOUT_MS = 5000;

const unsigned long OK_DISPLAY_MS    = 650;
const unsigned long NO_DISPLAY_MS    = 900;
const unsigned long SCORE_DISPLAY_MS = 1500;


// ------------------------------------------------------------
// 5x7 FONT
// ------------------------------------------------------------

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


void setDirectionRelay(int8_t direction)
{
  allRelaysOff();

  switch (direction)
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

    default:
      break;
  }
}


// ============================================================
// DISPLAY HELPERS
// ============================================================

void setPixel(uint8_t logicalRow, uint8_t logicalCol, bool state = true)
{
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


void clearDisplay()
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
    uint8_t bits =
      getGlyphRow(c, row);

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


uint8_t getTextWidth(const char* text)
{
  uint8_t length =
    strlen(text);

  if (length == 0)
  {
    return 0;
  }

  return (length * 6) - 1;
}


void showText(const char* text)
{
  beginDisplayFrame();

  uint8_t width =
    getTextWidth(text);

  int8_t x =
    (32 - width) / 2;

  for (
    uint8_t i = 0;
    text[i] != '\0';
    i++
  )
  {
    drawCharacter(
      text[i],
      x
    );

    x += 6;
  }

  endDisplayFrame();
}


void showNumber(uint8_t number)
{
  char buffer[4];

  itoa(
    number,
    buffer,
    10
  );

  showText(buffer);
}


void showCountdown()
{
  showText("3");
  delay(START_COUNTDOWN_MS);

  showText("2");
  delay(START_COUNTDOWN_MS);

  showText("1");
  delay(START_COUNTDOWN_MS);

  clearDisplay();
}


// ============================================================
// EEPROM CALIBRATION
// ============================================================

uint16_t calculateChecksum(const StoredCalibration& data)
{
  uint16_t sum =
    data.magic +
    data.version;

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    sum +=
      (uint16_t)data.baseline[i];

    sum +=
      (uint16_t)data.hand[i];
  }

  return sum;
}


bool loadCalibrationFromEEPROM()
{
  StoredCalibration data;

  EEPROM.get(
    EEPROM_ADDRESS,
    data
  );

  bool valid =
    data.magic == EEPROM_MAGIC &&
    data.version == EEPROM_VERSION &&
    data.checksum == calculateChecksum(data);

  if (!valid)
  {
    return false;
  }


  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    baseline[i] =
      data.baseline[i];

    handValue[i] =
      data.hand[i];

    deltaValue[i] =
      handValue[i] -
      baseline[i];

    baselineQ8[i] =
      ((long)baseline[i]) << 8;


    if (
      deltaValue[i] > -2 &&
      deltaValue[i] < 2
    )
    {
      return false;
    }
  }


#if DEBUG_SERIAL
  DBG_PRINTLN(F("EEPROM calibration loaded:"));

  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    DBG_PRINT(sensorNames[i]);

    DBG_PRINT(F(" base="));
    DBG_PRINT(baseline[i]);

    DBG_PRINT(F(" hand="));
    DBG_PRINT(handValue[i]);

    DBG_PRINT(F(" delta="));
    DBG_PRINTLN(deltaValue[i]);
  }

  DBG_PRINTLN("");
#endif

  return true;
}


// ============================================================
// ADC + FILTER + SCORE
// ============================================================

int readAnalogAveraged(uint8_t pin)
{
  // Discard first conversion after changing ADC channel.
  analogRead(pin);

  long total = 0;

  for (
    uint8_t i = 0;
    i < ADC_AVERAGE_SAMPLES;
    i++
  )
  {
    total +=
      analogRead(pin);
  }

  return
    total / ADC_AVERAGE_SAMPLES;
}


int16_t calculateScorePercent(
  uint8_t sensorId,
  int value
)
{
  int delta =
    deltaValue[sensorId];

  if (
    delta > -2 &&
    delta < 2
  )
  {
    return 0;
  }


  long numerator =
    (long)(
      value -
      baseline[sensorId]
    ) * 100L;

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


  return
    (int16_t)score;
}


void recalculateAllScores()
{
  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    scorePercent[i] =
      calculateScorePercent(
        i,
        filteredValue[i]
      );
  }
}


// ------------------------------------------------------------
// Slow baseline tracking.
//
// IMPORTANT:
// - Only RAM baseline changes.
// - EEPROM calibration remains untouched.
// - Never track the currently latched sensor.
// - Never track a sensor close to a real press.
// ------------------------------------------------------------

void updateAdaptiveBaselines()
{
  if (
    millis() - lastBaselineTrack
    < BASELINE_TRACK_INTERVAL_MS
  )
  {
    return;
  }


  lastBaselineTrack =
    millis();


  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    if (i == latchedSensor)
    {
      continue;
    }


    if (
      scorePercent[i]
      > BASELINE_TRACK_MAX_SCORE
    )
    {
      continue;
    }


    long targetQ8 =
      ((long)filteredValue[i]) << 8;

    long proposedQ8 =
      baselineQ8[i]
      +
      (
        targetQ8 -
        baselineQ8[i]
      )
      /
      BASELINE_TRACK_DIVISOR;

    int16_t proposedBaseline =
      (int16_t)(
        proposedQ8 >> 8
      );

    int16_t proposedDelta =
      handValue[i] -
      proposedBaseline;


    if (
      proposedDelta > -MIN_LIVE_DELTA &&
      proposedDelta < MIN_LIVE_DELTA
    )
    {
      continue;
    }


    baselineQ8[i] =
      proposedQ8;

    baseline[i] =
      proposedBaseline;

    deltaValue[i] =
      proposedDelta;
  }


  recalculateAllScores();
}


void readAllSensors(
  bool allowBaselineTracking
)
{
  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    rawValue[i] =
      readAnalogAveraged(
        sensorPins[i]
      );


    if (!filtersInitialized)
    {
      filteredValue[i] =
        rawValue[i];
    }
    else
    {
      filteredValue[i] =
        (
          (long)filteredValue[i]
          *
          FILTER_OLD_WEIGHT
          +
          rawValue[i]
        )
        /
        FILTER_TOTAL_WEIGHT;
    }
  }


  filtersInitialized = true;

  recalculateAllScores();


  if (allowBaselineTracking)
  {
    updateAdaptiveBaselines();
  }
}


// ============================================================
// WINNER SELECTION
// ============================================================

int8_t findValidWinner(
  uint8_t allowedMask
)
{
  int8_t bestSensor = -1;

  int16_t bestScore = -32768;
  int16_t secondScore = -32768;


  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    if (
      (
        allowedMask &
        (1 << i)
      )
      == 0
    )
    {
      continue;
    }


    int16_t score =
      scorePercent[i];


    if (score > bestScore)
    {
      secondScore =
        bestScore;

      bestScore =
        score;

      bestSensor =
        i;
    }
    else if (score > secondScore)
    {
      secondScore =
        score;
    }
  }


  if (
    bestSensor < 0 ||
    bestScore < TRIGGER_SCORE
  )
  {
    return -1;
  }


  if (
    secondScore >= TRIGGER_SCORE &&
    (
      bestScore -
      secondScore
    )
    < WINNER_MARGIN
  )
  {
    return -1;
  }


  return
    bestSensor;
}


// ============================================================
// NEUTRAL TEST
// ============================================================

bool allowedSensorsNeutral(
  uint8_t allowedMask
)
{
  for (uint8_t i = 0; i < SENSOR_COUNT; i++)
  {
    if (
      (
        allowedMask &
        (1 << i)
      )
      == 0
    )
    {
      continue;
    }


    if (
      scorePercent[i]
      >= RELEASE_SCORE
    )
    {
      return false;
    }
  }


  return true;
}


// ============================================================
// EVENT MACHINE HELPERS
// ============================================================

void resetEventState()
{
  latchedSensor = -1;

  candidateSensor = -1;
  candidateSince = 0;

  releaseSince = 0;
  neutralSince = 0;

  eventArmed = false;
}


#if DEBUG_SERIAL
void printEventScores()
{
  DBG_PRINT(F("SCORES  START="));
  DBG_PRINT(scorePercent[START_ID]);

  DBG_PRINT(F("  UP="));
  DBG_PRINT(scorePercent[UP_ID]);

  DBG_PRINT(F("  DOWN="));
  DBG_PRINT(scorePercent[DOWN_ID]);

  DBG_PRINT(F("  LEFT="));
  DBG_PRINT(scorePercent[LEFT_ID]);

  DBG_PRINT(F("  RIGHT="));
  DBG_PRINTLN(scorePercent[RIGHT_ID]);
}
#endif


// ============================================================
// WAIT FOR ONE COMPLETE PRESS + RELEASE EVENT
//
// Returns:
//   sensor ID on success
//   -1 = press timeout
//   -2 = accepted press did not release before release timeout
//
// IMPORTANT:
// resetEventState() is NOT called here.
// The player turn calls it once before the whole sequence,
// preserving the validated event lifecycle between positions.
// ============================================================

int8_t waitForPressReleaseEvent(
  uint8_t allowedMask,
  unsigned long pressTimeoutMs,
  bool lampFeedback
)
{
  unsigned long pressWaitStarted =
    millis();


  while (true)
  {
    if (
      pressTimeoutMs > 0 &&
      millis() - pressWaitStarted
      >= pressTimeoutMs
    )
    {
      allRelaysOff();

      candidateSensor = -1;
      candidateSince = 0;

      return -1;
    }


    readAllSensors(true);


    // --------------------------------------------------------
    // REARM: require a real neutral interval.
    // --------------------------------------------------------

    if (!eventArmed)
    {
      allRelaysOff();


      if (
        allowedSensorsNeutral(
          allowedMask
        )
      )
      {
        if (neutralSince == 0)
        {
          neutralSince =
            millis();
        }


        if (
          millis() - neutralSince
          >= REARM_NEUTRAL_MS
        )
        {
          eventArmed =
            true;

          candidateSensor = -1;
          candidateSince = 0;

          DBG_PRINTLN(F("ARMED"));
        }
      }
      else
      {
        neutralSince = 0;
      }


      continue;
    }


    // --------------------------------------------------------
    // ARMED: find one stable winner.
    // --------------------------------------------------------

    int8_t winner =
      findValidWinner(
        allowedMask
      );


    if (winner < 0)
    {
      candidateSensor = -1;
      candidateSince = 0;

      continue;
    }


    if (
      candidateSensor
      != winner
    )
    {
      candidateSensor =
        winner;

      candidateSince =
        millis();

      continue;
    }


    if (
      millis() - candidateSince
      < PRESS_STABLE_MS
    )
    {
      continue;
    }


    // --------------------------------------------------------
    // PRESS accepted.
    // --------------------------------------------------------

    latchedSensor =
      winner;

    inputEventCounter++;

    eventArmed = false;

    candidateSensor = -1;
    candidateSince = 0;

    releaseSince = 0;


#if DEBUG_SERIAL
    DBG_PRINT(F("PRESS #"));
    DBG_PRINT(inputEventCounter);

    DBG_PRINT(F("  "));
    DBG_PRINT(sensorNames[latchedSensor]);

    DBG_PRINT(F("  score="));
    DBG_PRINTLN(
      scorePercent[latchedSensor]
    );

    printEventScores();
#endif


    if (
      lampFeedback &&
      latchedSensor != START_ID
    )
    {
      setDirectionRelay(
        latchedSensor
      );
    }
    else
    {
      allRelaysOff();
    }


    // --------------------------------------------------------
    // LATCHED:
    // Ignore all other sensors.
    // Only THIS sensor can finish THIS event.
    // --------------------------------------------------------

    unsigned long releaseWaitStarted =
      millis();


    while (
      millis() - releaseWaitStarted
      < RELEASE_TIMEOUT_MS
    )
    {
      // Do not baseline-track while the accepted hand is down.
      readAllSensors(false);


      if (
        lampFeedback &&
        latchedSensor != START_ID
      )
      {
        setDirectionRelay(
          latchedSensor
        );
      }
      else
      {
        allRelaysOff();
      }


      if (
        scorePercent[latchedSensor]
        < RELEASE_SCORE
      )
      {
        if (releaseSince == 0)
        {
          releaseSince =
            millis();
        }


        if (
          millis() - releaseSince
          >= RELEASE_STABLE_MS
        )
        {
          int8_t completedSensor =
            latchedSensor;


#if DEBUG_SERIAL
          DBG_PRINT(F("RELEASE #"));
          DBG_PRINT(inputEventCounter);

          DBG_PRINT(F("  "));
          DBG_PRINTLN(
            sensorNames[completedSensor]
          );
#endif


          latchedSensor = -1;

          releaseSince = 0;
          neutralSince = 0;

          candidateSensor = -1;
          candidateSince = 0;

          eventArmed = false;

          allRelaysOff();

          return
            completedSensor;
        }
      }
      else
      {
        releaseSince = 0;
      }
    }


    // Accepted press never released.
    DBG_PRINTLN(
      F("RELEASE TIMEOUT")
    );

    latchedSensor = -1;
    releaseSince = 0;
    neutralSince = 0;

    eventArmed = false;

    allRelaysOff();

    return -2;
  }
}


// ============================================================
// GAME SEQUENCE
// ============================================================

uint8_t randomDirection()
{
  return random(
    UP_ID,
    RIGHT_ID + 1
  );
}


void startNewSequence()
{
  sequenceLength = 1;
  completedRounds = 0;

  sequence[0] =
    randomDirection();
}


bool appendDirection()
{
  if (
    sequenceLength
    >= MAX_SEQUENCE_LENGTH
  )
  {
    return false;
  }


  sequence[sequenceLength] =
    randomDirection();

  sequenceLength++;

  return true;
}


void playSequence()
{
  allRelaysOff();
  clearDisplay();


  DBG_PRINT(F("SHOW LEVEL "));
  DBG_PRINTLN(sequenceLength);


  for (
    uint8_t i = 0;
    i < sequenceLength;
    i++
  )
  {
    uint8_t direction =
      sequence[i];


#if DEBUG_SERIAL
    DBG_PRINT(F("  "));
    DBG_PRINT(i + 1);

    DBG_PRINT(F(": "));
    DBG_PRINTLN(
      sensorNames[direction]
    );
#endif


    setDirectionRelay(
      direction
    );

    delay(
      SEQUENCE_ON_MS
    );


    allRelaysOff();

    delay(
      SEQUENCE_GAP_MS
    );
  }
}


// ============================================================
// GAME FEEDBACK
// ============================================================

void showRoundSuccess()
{
  allRelaysOff();

  showText("OK");

  delay(
    OK_DISPLAY_MS
  );

  clearDisplay();
}


void showGameFailure(
  const char* label
)
{
  allRelaysOff();

  showText(label);

  delay(
    NO_DISPLAY_MS
  );


  showNumber(
    completedRounds
  );

  delay(
    SCORE_DISPLAY_MS
  );

  clearDisplay();
}


// ============================================================
// GAME LOOP
// ============================================================

void runGame()
{
  randomSeed(
    micros()
    ^
    (
      (unsigned long)
      rawValue[START_ID]
      << 10
    )
  );


  DBG_PRINTLN(F(""));
  DBG_PRINTLN(F("NEW GAME"));


  allRelaysOff();

  showCountdown();

  startNewSequence();


  while (true)
  {
    // --------------------------------------------------------
    // Level number.
    // --------------------------------------------------------

    showNumber(
      sequenceLength
    );

    delay(
      LEVEL_DISPLAY_MS
    );

    clearDisplay();


    // --------------------------------------------------------
    // Computer sequence: lamps only.
    // --------------------------------------------------------

    playSequence();


    // --------------------------------------------------------
    // Player turn.
    // --------------------------------------------------------

    showText("GO");

    delay(
      GO_DISPLAY_MS
    );

    clearDisplay();


    // Reset event state ONCE for the whole player round.
    // Do not reset between expected directions.
    resetEventState();


    // Refresh filters and adaptive resting baseline before
    // the first press can arm.
    readAllSensors(true);


    DBG_PRINTLN(
      F("STATE: PLAYER TURN")
    );


    for (
      uint8_t position = 0;
      position < sequenceLength;
      position++
    )
    {
      int8_t playerDirection =
        waitForPressReleaseEvent(
          DIRECTION_SENSOR_MASK,
          INPUT_TIMEOUT_MS,
          PLAYER_LAMP_FEEDBACK != 0
        );


      if (playerDirection == -1)
      {
        DBG_PRINTLN(
          F("PLAYER PRESS TIMEOUT")
        );

        showGameFailure("TO");

        return;
      }


      if (playerDirection == -2)
      {
        DBG_PRINTLN(
          F("PLAYER RELEASE TIMEOUT")
        );

        showGameFailure("TO");

        return;
      }


      uint8_t expectedDirection =
        sequence[position];


#if DEBUG_SERIAL
      DBG_PRINT(F("CHECK "));
      DBG_PRINT(position + 1);

      DBG_PRINT('/');
      DBG_PRINT(sequenceLength);

      DBG_PRINT(F("  EXPECTED="));
      DBG_PRINT(
        sensorNames[expectedDirection]
      );

      DBG_PRINT(F("  GOT="));
      DBG_PRINTLN(
        sensorNames[playerDirection]
      );
#endif


      if (
        playerDirection
        != expectedDirection
      )
      {
        DBG_PRINTLN(
          F("WRONG INPUT")
        );

        showGameFailure("WR");

        return;
      }
    }


    // --------------------------------------------------------
    // Entire round correct.
    // --------------------------------------------------------

    completedRounds++;


    DBG_PRINT(F("ROUND OK. SCORE="));
    DBG_PRINTLN(completedRounds);


    showRoundSuccess();


    if (
      sequenceLength
      >= MAX_SEQUENCE_LENGTH
    )
    {
      showNumber(
        completedRounds
      );

      delay(2500);

      clearDisplay();

      return;
    }


    appendDirection();

    delay(450);
  }
}


// ============================================================
// IDLE / START
// ============================================================

void runIdle()
{
  showText("START");

  allRelaysOff();

  resetEventState();


  DBG_PRINTLN(
    F("STATE: IDLE")
  );


  while (true)
  {
    int8_t event =
      waitForPressReleaseEvent(
        START_SENSOR_MASK,
        0,       // No timeout while waiting for START.
        false    // START has no lamp.
      );


    if (event != START_ID)
    {
      resetEventState();
      continue;
    }


    DBG_PRINTLN(F("START"));


    allRelaysOff();
    clearDisplay();


    runGame();


    // Game finished.
    // Return to a fresh START gesture.
    allRelaysOff();

    resetEventState();

    showText("START");

    DBG_PRINTLN(
      F("STATE: IDLE")
    );
  }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  DBG_BEGIN(115200);


  pinMode(
    RELAY_UP_PIN,
    OUTPUT
  );

  pinMode(
    RELAY_DOWN_PIN,
    OUTPUT
  );

  pinMode(
    RELAY_LEFT_PIN,
    OUTPUT
  );

  pinMode(
    RELAY_RIGHT_PIN,
    OUTPUT
  );

  allRelaysOff();


  matrix.begin();

  matrix.control(
    MD_MAX72XX::INTENSITY,
    3
  );

  clearDisplay();


  if (
    !loadCalibrationFromEEPROM()
  )
  {
    allRelaysOff();

    showText("CAL");

    DBG_PRINTLN(
      F("ERROR: EEPROM calibration invalid.")
    );


    while (true)
    {
      delay(1000);
    }
  }


  // Prime sensor filters from the saved calibration.
  readAllSensors(true);


  DBG_PRINTLN(
    F("GLASS MEMORY GAME V4 READY")
  );

  DBG_PRINTLN(
    F("Validated latched input detector + adaptive baseline")
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  runIdle();
}
