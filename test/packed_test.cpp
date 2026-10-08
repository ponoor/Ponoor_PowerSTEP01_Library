// Host-side test for packed daisy-chain transfers. A small simulator of a
// powerSTEP01 chain (chip_sim.h) sits behind a mocked SPI port; the same
// operations are run through the immediate API and the
// prepare*()/performPrepared() API and the resulting chip state / read values
// are compared.
// Build: g++ -std=c++11 -Itest/mock -Itest -Isrc test/packed_test.cpp src/*.cpp -o packed_test
#include <cstdio>
#include <vector>
#include <string>
#include "Arduino.h"
#include "SPI.h"
#include "Ponoor_PowerSTEP01Library.h"
#include "chip_sim.h"

SPIClass SPI;

static int csLowCount = 0;
void digitalWrite(int pin, int value) { if (pin == 10 && value == LOW) csLowCount++; }
int digitalRead(int) { return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static const int N = 4;
static Chip chips[N];
static int frames = 0;

static void chainTransfer(uint8_t *buf, size_t n) {
  frames++;
  for (size_t i = 0; i < n; i++) buf[i] = chips[i].xfer(buf[i]);
}

int main()
{
  SPI.hook = chainTransfer;
  powerSTEP d0(0, 10, 6), d1(1, 10, 6), d2(2, 10, 6), d3(3, 10, 6);
  powerSTEP *drv[N] = { &d0, &d1, &d2, &d3 };
  int32_t pos[N] = { 12345, -1234, 0, -2097151 };
  for (int i = 0; i < N; i++) chips[i].regs[0x01] = (uint32_t)pos[i] & 0x3FFFFF;
  for (int i = 0; i < N; i++) { chips[i].regs[0x1B] = 0xE401; chips[i].regs[0x07] = 0x41; }
  chips[1].regs[0x1B] = 0x1234;

  // --- immediate reference ---
  long refPos[N]; int refStatus[N];
  frames = 0;
  for (int i = 0; i < N; i++) refPos[i] = drv[i]->getPos();
  int immediateFrames = frames;
  for (int i = 0; i < N; i++) refStatus[i] = drv[i]->getStatus();
  for (int i = 0; i < N; i++) { CHECK(refPos[i] == pos[i]); if (refPos[i] != pos[i]) printf("  i=%d ref=%ld exp=%d\n", i, refPos[i], (int)pos[i]); }

  // --- packed reads ---
  frames = 0; csLowCount = 0;
  for (int i = 0; i < N; i++) drv[i]->prepareGetPos();
  CHECK(frames == 0);  // preparing does not touch SPI
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 4);
  CHECK(csLowCount == 4);
  for (int i = 0; i < N; i++) CHECK(drv[i]->preparedPos() == refPos[i]);
  printf("getPos: immediate %d frames, packed %d frames\n", immediateFrames, frames);

  for (int i = 0; i < N; i++) drv[i]->prepareGetStatus();
  CHECK(powerSTEP::performPrepared());
  for (int i = 0; i < N; i++) CHECK(drv[i]->preparedStatus() == (int)(refStatus[i] & 0xFFFF));
  CHECK(drv[1]->preparedStatus() == 0x1234);

  // register read of a 10-bit register is masked to the register width
  chips[2].regs[0x07] = 0xFFFF;
  for (int i = 0; i < N; i++) drv[i]->prepareGetParam(MAX_SPEED);
  CHECK(powerSTEP::performPrepared());
  CHECK(drv[2]->preparedResult() == 0x3FF);
  CHECK(drv[0]->preparedResult() == 0x41);

  // 5-bit ADC_OUT: masked to 5 bits
  chips[3].regs[0x12] = 0xFF;
  for (int i = 0; i < N; i++) drv[i]->prepareGetParam(ADC_OUT);
  CHECK(powerSTEP::performPrepared());
  CHECK(drv[3]->preparedResult() == 0x1F);

  // unknown register cancels the staged command
  drv[0]->prepareGetParam(0x1F);
  frames = 0;
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 0);

  // --- mixed commands: compare against immediate execution ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->run(FWD, 1000.0f);
  drv[1]->move(REV, 5000);
  drv[2]->goTo(-100);
  drv[3]->softStop();
  drv[0]->setParam(MAX_SPEED, 0x55);
  drv[1]->setParam(GATECFG1, SR_980V_us);
  std::vector<std::string> ref[N];
  for (int i = 0; i < N; i++) { ref[i] = chips[i].log; chips[i].log.clear(); }

  frames = 0;
  drv[0]->prepareRun(FWD, 1000.0f);
  drv[1]->prepareMove(REV, 5000);
  drv[2]->prepareGoTo(-100);
  drv[3]->prepareSoftStop();
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 4);
  drv[0]->prepareSetParam(MAX_SPEED, 0x55);
  drv[1]->prepareSetParam(GATECFG1, SR_980V_us);
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 7);
  for (int i = 0; i < N; i++) {
    CHECK(chips[i].log == ref[i]);
    if (chips[i].log != ref[i]) {
      for (auto &l : chips[i].log) printf("  packed[%d]: %s\n", i, l.c_str());
      for (auto &l : ref[i]) printf("  ref[%d]:    %s\n", i, l.c_str());
    }
  }

  // --- the remaining commands ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->goToDir(FWD, 777); drv[1]->hardStop(); drv[2]->softHiZ(); drv[3]->hardHiZ();
  drv[0]->runRaw(REV, 0xFFFFF);
  for (int i = 0; i < N; i++) { ref[i] = chips[i].log; chips[i].log.clear(); }
  drv[0]->prepareGoToDir(FWD, 777); drv[1]->prepareHardStop();
  drv[2]->prepareSoftHiZ(); drv[3]->prepareHardHiZ();
  CHECK(powerSTEP::performPrepared());
  drv[0]->prepareRunRaw(REV, 0xFFFFF);
  CHECK(powerSTEP::performPrepared());
  for (int i = 0; i < N; i++) CHECK(chips[i].log == ref[i]);

  // --- cancel with prepareNop and nothing staged ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->prepareRun(FWD, 1000.0f);
  drv[0]->prepareNop();
  frames = 0;
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 0);
  CHECK(chips[0].log.empty());
  // prepared state is cleared after perform
  drv[1]->prepareHardStop();
  CHECK(powerSTEP::performPrepared());
  frames = 0;
  CHECK(powerSTEP::performPrepared());
  CHECK(frames == 0);

  // --- goUntilRaw() clamps to 20 bits ---
  for (int i = 0; i < N; i++) chips[i].log.clear();
  drv[0]->goUntilRaw(RESET_ABSPOS, FWD, 0x3FFFFF);
  CHECK(chips[0].log.size() == 1);
  CHECK(chips[0].log[0] == "cmd 131 " + std::to_string(0xFFFFF));

  // --- SPI clock clamp ---
  powerSTEP::setSPIClock(10000000);
  drv[0]->getPos();
  CHECK(SPI.lastClock == 5000000);
  powerSTEP::setSPIClock(1000000);
  drv[0]->getPos();
  CHECK(SPI.lastClock == 1000000);
  powerSTEP::setSPIClock(4000000);

  // --- an instance on another CS pin makes the chain unsendable ---
  powerSTEP odd(4, 11, 6);
  d0.prepareHardStop();
  frames = 0;
  CHECK(!powerSTEP::performPrepared());
  CHECK(frames == 0);

  printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
