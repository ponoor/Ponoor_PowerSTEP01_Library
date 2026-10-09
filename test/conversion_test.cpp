// Host-side test for unit conversion functions and register handling.
// Build: g++ -std=c++11 -Itest/mock -Isrc test/conversion_test.cpp src/*.cpp -o conversion_test
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Arduino.h"
#include "SPI.h"
#define private public
#include "Ponoor_PowerSTEP01Library.h"
#undef private
#include "chip_sim.h"

SPIClass SPI;
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static Chip chip;
static void singleChip(uint8_t *buf, size_t n) {
  for (size_t i = 0; i < n; i++) buf[i] = chip.xfer(buf[i]);
}

int main()
{
  powerSTEP d(0, 10, 6);

  // --- unit conversions ---
  int roundTripErrors = 0;
  for (unsigned long n = 1; n <= 0xFFE; n++) {
    if (d.accCalc(d.accParse(n)) != n) roundTripErrors++;
  }
  for (unsigned long n = 1; n <= 0xFFF; n++) {
    if (d.decCalc(d.decParse(n)) != n) roundTripErrors++;
  }
  printf("round trip errors: %d\n", roundTripErrors);
  CHECK(roundTripErrors == 0);

  CHECK(fabs(d.accParse(68) - 989.5f) < 0.1f);
  CHECK(fabs(d.decParse(0xFFF) - 59590.0f) < 0.5f);
  CHECK(d.accCalc(1e6f) == 0xFFE);
  CHECK(d.decCalc(1e6f) == 0xFFF);   // 0xFFF is valid for DEC (datasheet 11.1.6)
  CHECK(d.accCalc(0.0f) == 1);
  CHECK(d.decCalc(0.0f) == 1);
  CHECK(d.accCalc(-5.0f) == 1);
  CHECK(d.decCalc(-5.0f) == 1);
  CHECK(d.accCalc(59590.0f) == 0xFFE);
  CHECK(d.maxSpdCalc(0.0f) == 1);    // 0x000 is reserved
  CHECK(d.maxSpdCalc(-100.0f) == 1);
  CHECK(d.maxSpdCalc(1e6f) == 0x3FF);
  CHECK(d.maxSpdCalc(991.8f) == 0x41);

  // --- SR_* constants: IGATE (7:5) and TCC (4:0) against datasheet Table 11 ---
  struct { int sr; int igateMa; int tccNs; } sr[] = {
    { SR_114V_us, 8, 3125 }, { SR_220V_us, 16, 1625 }, { SR_400V_us, 24, 1000 },
    { SR_520V_us, 32, 875 }, { SR_790V_us, 64, 500 }, { SR_980V_us, 96, 375 },
  };
  const int igateTable[8] = { 4, 4, 8, 16, 24, 32, 64, 96 };  // Table 34
  for (auto &e : sr) {
    CHECK((e.sr & ~0xFF) == 0);
    CHECK(igateTable[(e.sr >> 5) & 7] == e.igateMa);
    CHECK((((e.sr & 0x1F) + 1) * 125) == e.tccNs);  // Table 35
  }

  // --- registers against a simulated chip ---
  SPI.hook = singleChip;  // a chain of one chip

  // getSlewRate() returns what setSlewRate() wrote (GATECFG1 7:0), and leaves
  // the other bits alone.
  chip.regs[0x18] = 0x0700;
  d.setSlewRate(SR_980V_us);
  CHECK(chip.regs[0x18] == (0x0700u | SR_980V_us));
  CHECK(d.getSlewRate() == SR_980V_us);
  d.setSlewRate(SR_114V_us);
  CHECK(d.getSlewRate() == SR_114V_us);
  CHECK(chip.regs[0x18] == (0x0700u | SR_114V_us));
  chip.regs[0x1A] = 0x2FFF;  // CONFIG must not leak into getSlewRate()
  CHECK(d.getSlewRate() == SR_114V_us);

  // setFullSpeedRaw() keeps BOOST_MODE, getFullSpeedRaw() excludes it.
  chip.regs[0x15] = 0x0400 | 0x027;
  d.setFullSpeedRaw(0x123);
  CHECK(chip.regs[0x15] == (0x0400u | 0x123));
  CHECK(d.getFullSpeedRaw() == 0x123);
  CHECK(d.getBoostMode());
  d.setFullSpeedRaw(0xFFFF);  // upper bits must not touch BOOST_MODE
  CHECK(chip.regs[0x15] == (0x0400u | 0x3FF));
  d.setBoostMode(false);
  CHECK(chip.regs[0x15] == 0x3FF);
  CHECK(!d.getBoostMode());
  d.setFullSpeedRaw(0x055);
  CHECK(chip.regs[0x15] == 0x055);  // BOOST_MODE stays cleared
  d.setBoostMode(true);
  CHECK(chip.regs[0x15] == (0x0400u | 0x055));
  CHECK(fabs(d.getFullSpeed() - (0x055 + 0.5f) * 15.258789f) < 0.01f);

  // MIN_SPEED keeps LSPD_OPT.
  chip.regs[0x08] = 0x1000;
  d.setMinSpeedRaw(0x0AB);
  CHECK(chip.regs[0x08] == 0x10AB);

  // TVAL setters write 7 bits.
  d.setRunTVAL(0xFF);
  CHECK(chip.regs[0x0A] == 0x7F);
  d.setAccTVAL(0x80);
  CHECK(chip.regs[0x0B] == 0);

  // ADC_OUT is 5 bits; OCD_TH / STALL_TH / K_THERM are masked.
  chip.regs[0x12] = 0xFF;
  CHECK(d.getParam(ADC_OUT) == 0x1F);
  d.setOCThreshold(0xFF);
  CHECK(chip.regs[0x13] == 0x1F);

  // getDec() is converted with the DEC function.
  chip.regs[0x06] = 0xFFF;
  CHECK(fabs(d.getDec() - 59590.0f) < 0.5f);
  d.setDec(1e6f);
  CHECK(chip.regs[0x06] == 0xFFF);
  d.setAcc(1e6f);
  CHECK(chip.regs[0x05] == 0xFFE);
  d.setMaxSpeed(0.0f);
  CHECK(chip.regs[0x07] == 1);

  // Constants.
  CHECK(CURRENT_MODE == STEP_MODE_CM_VM);
  CHECK(CMD_GET_PARAM == 0x20 && CMD_HARD_HIZ == 0xA8);

  printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
