/****************************************************************************
 * PackedCommands.ino
 * Packed daisy-chain transfer example for the Ponoor PowerSTEP01 Library
 *
 * Compares the time needed for one servo-style cycle (read the position of
 * every motor, then update its speed) with the regular API and with the
 * packed API (prepare*() / performPrepared()).
 *
 * With the regular API every call shifts the whole chain, so talking to N
 * chips costs N*N bytes per byte of command. The packed API stages one
 * command per chip and sends all of them together in at most 4 frames.
 *
 * Restriction: all drivers must share the same CS pin and SPI port.
 *
 * Based on the gantry example by Mike Hord @ SparkFun Electronics.
 * This code is beerware; if you see me (or any other SparkFun employee) at the
 * local, and you've found our code helpful, please buy us a round!
 ****************************************************************************/

#include <Ponoor_PowerSTEP01Library.h>
#include <SPI.h>

// Number of powerSTEP01 chips in the daisy chain (4 for the STEP400).
#define NUM_BOARDS 4

#define CS_PIN    10
#define RESET_PIN 8

// P control parameters: speed [steps/s] = KP * position error [steps].
#define KP         2.0f
#define SERVO_MAX_SPEED  1000.0f
#define TARGET_POS 20000L

// Numbering starts from the board farthest from the controller and counts
//  up from 0. The objects are created in setup() so that the number of
//  boards is set by NUM_BOARDS alone.
powerSTEP *boards[NUM_BOARDS];

// Converts a position error into a signed speed command.
static void pControl(long position, long target, byte *dir, float *speed)
{
  float v = KP * (float)(target - position);
  if (v > SERVO_MAX_SPEED) v = SERVO_MAX_SPEED;
  if (v < -SERVO_MAX_SPEED) v = -SERVO_MAX_SPEED;
  *dir = (v >= 0) ? FWD : REV;
  *speed = fabs(v);
}

// One servo cycle with the regular API: 2 calls per board, each of which
//  shifts the whole chain.
void servoImmediate()
{
  for (int i = 0; i < NUM_BOARDS; i++)
  {
    byte dir;
    float speed;
    pControl(boards[i]->getPos(), TARGET_POS, &dir, &speed);
    boards[i]->run(dir, speed);
  }
}

// One servo cycle with the packed API: one transfer to read all positions,
//  one transfer to update all speeds.
void servoPacked()
{
  for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareGetPos();
  powerSTEP::performPrepared();

  for (int i = 0; i < NUM_BOARDS; i++)
  {
    byte dir;
    float speed;
    pControl(boards[i]->preparedPos(), TARGET_POS, &dir, &speed);
    boards[i]->prepareRun(dir, speed);
  }
  powerSTEP::performPrepared();
}

void setup()
{
  Serial.begin(115200);

  // The library does not set up the SPI port and pins for you.
  pinMode(RESET_PIN, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(RESET_PIN, LOW);
  digitalWrite(RESET_PIN, HIGH);
  SPI.begin();
  SPI.setDataMode(SPI_MODE3);

  // Create every object before the first transfer: the library needs to know
  //  the length of the whole chain.
  for (int i = 0; i < NUM_BOARDS; i++)
  {
    boards[i] = new powerSTEP(i, CS_PIN, RESET_PIN);
  }

  for (int i = 0; i < NUM_BOARDS; i++)
  {
    boards[i]->SPIPortConnect(&SPI);
    boards[i]->setOCThreshold(8);  // 5-bit threshold: (8 + 1) * 31.25 mV = 281.25 mV (default)
    boards[i]->setSlewRate(SR_520V_us);
    boards[i]->setRunKVAL(64);
    boards[i]->setAccKVAL(64);
    boards[i]->setDecKVAL(64);
    boards[i]->setHoldKVAL(16);
    boards[i]->setMaxSpeed(SERVO_MAX_SPEED);
    boards[i]->setAcc(2000);
    boards[i]->setDec(2000);
    boards[i]->setPos(0);
  }

  // Measure one servo cycle with each API.
  const int repeat = 20;
  unsigned long t0 = micros();
  for (int n = 0; n < repeat; n++) servoImmediate();
  unsigned long tImmediate = (micros() - t0) / repeat;

  t0 = micros();
  for (int n = 0; n < repeat; n++) servoPacked();
  unsigned long tPacked = (micros() - t0) / repeat;

  Serial.print("Boards: ");
  Serial.println(NUM_BOARDS);
  Serial.print("Regular API, getPos() + run() on every board: ");
  Serial.print(tImmediate);
  Serial.println(" us/cycle");
  Serial.print("Packed API, prepareGetPos() + prepareRun():   ");
  Serial.print(tPacked);
  Serial.println(" us/cycle");
}

void loop()
{
  // Servo loop using the packed API, every 10 ms.
  static unsigned long last = 0;
  if (millis() - last >= 10)
  {
    last = millis();
    servoPacked();
  }
}
