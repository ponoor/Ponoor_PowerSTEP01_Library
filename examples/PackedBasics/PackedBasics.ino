/****************************************************************************
 * PackedBasics.ino
 * Minimal packed daisy-chain example for the Ponoor PowerSTEP01 Library
 *
 * Shows the basic prepare -> perform -> read-result flow with a chain of
 * NUM_BOARDS chips, controlled from the Serial Monitor (115200 baud):
 *
 *   s  Read position and status of every board with two packed transfers
 *   r  Run every board in a different direction/speed with one transfer
 *   m  Move every board by 1000 steps with one transfer
 *   x  Soft stop every board with one transfer
 *   h  Hard stop board 0 and soft stop the others in the same transfer
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
#define CS_PIN     10
#define RESET_PIN  8

// Board 0 is the one farthest from the controller.
powerSTEP *boards[NUM_BOARDS];

// Sends the staged commands and reports a chain configuration error.
static void perform()
{
  if (!powerSTEP::performPrepared())
  {
    Serial.println("performPrepared() failed: all boards must share the same CS pin and SPI port.");
  }
}

static void printState()
{
  // Stage a GetParam(ABS_POS) on every board and send them together.
  for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareGetPos();
  perform();
  long pos[NUM_BOARDS];
  for (int i = 0; i < NUM_BOARDS; i++) pos[i] = boards[i]->preparedPos();

  // A second transfer reads the status register of every board.
  for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareGetStatus();
  perform();

  for (int i = 0; i < NUM_BOARDS; i++)
  {
    Serial.print("Board ");
    Serial.print(i);
    Serial.print(": pos=");
    Serial.print(pos[i]);
    Serial.print(" status=0x");
    Serial.println(boards[i]->preparedStatus(), HEX);
  }
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
    boards[i]->setMaxSpeed(1000);
    boards[i]->setAcc(1000);
    boards[i]->setDec(1000);
  }

  Serial.println("s: state, r: run, m: move, x: soft stop, h: hard stop board 0");
}

void loop()
{
  if (!Serial.available()) return;

  switch (Serial.read())
  {
    case 's':
      printState();
      break;
    case 'r':
      // Different speed and direction for each board, one transfer.
      for (int i = 0; i < NUM_BOARDS; i++)
      {
        boards[i]->prepareRun((i % 2) ? REV : FWD, 200.0f * (i + 1));
      }
      perform();
      break;
    case 'm':
      for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareMove(FWD, 1000);
      perform();
      break;
    case 'x':
      for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareSoftStop();
      perform();
      break;
    case 'h':
      // Each board can get a different command in the same transfer.
      boards[0]->prepareHardStop();
      for (int i = 1; i < NUM_BOARDS; i++) boards[i]->prepareSoftStop();
      perform();
      break;
  }
}
