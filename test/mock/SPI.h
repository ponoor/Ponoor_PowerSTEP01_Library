// Minimal SPI mock for host-side tests. Transfers are delegated to a hook so
// that tests can record and answer packets.
#pragma once
#include "Arduino.h"
#include <functional>

#define SPI_MODE3 3

struct SPISettings {
  SPISettings(uint32_t c = 0, int = 0, int = 0) : clock(c) {}
  uint32_t clock;
};

class SPIClass {
public:
  void beginTransaction(SPISettings s) { lastClock = s.clock; }
  void endTransaction() {}
  void begin() {}
  void setDataMode(int) {}
  void transfer(void *buf, size_t n) { if (hook) hook((uint8_t *)buf, n); }
  std::function<void(uint8_t *, size_t)> hook;
  uint32_t lastClock = 0;
};

extern SPIClass SPI;
