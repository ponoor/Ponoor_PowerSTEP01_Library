// Host-side test for POWERSTEP01_MAX_DEVICES and the legacy command names.
// Build: g++ -std=c++11 -DPOWERSTEP01_MAX_DEVICES=2 -Itest/mock -Itest -Isrc test/limits_test.cpp src/*.cpp -o limits_test
#include <cstdio>
#include "Arduino.h"
#include "SPI.h"
#define POWERSTEP01_LEGACY_COMMAND_NAMES
#include "Ponoor_PowerSTEP01Library.h"

SPIClass SPI;
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static int frames = 0;

int main()
{
  SPI.hook = [](uint8_t *, size_t) { frames++; };

  // Legacy names map to the CMD_* values.
  CHECK(NOP == CMD_NOP && RUN == CMD_RUN && GO_UNTIL == CMD_GO_UNTIL && HARD_HIZ == CMD_HARD_HIZ);
  CHECK(SET_PARAM == 0x00 && GET_PARAM == 0x20 && RESET_DEVICE == 0xC0);

  // A chain within the limit works...
  powerSTEP a(0, 10, 6), b(1, 10, 6);
  a.hardStop();
  CHECK(frames == 1);
  a.prepareHardStop();
  CHECK(powerSTEP::performPrepared());

  // ...and a longer one is rejected.
  powerSTEP c(2, 10, 6);
  frames = 0;
  a.hardStop();
  CHECK(frames == 0);
  a.prepareHardStop();
  CHECK(!powerSTEP::performPrepared());
  CHECK(frames == 0);

  printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
