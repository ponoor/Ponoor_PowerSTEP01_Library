#include <SPI.h>
#include "Ponoor_PowerSTEP01Library.h"

int powerSTEP::_numBoards;
powerSTEP *powerSTEP::_instances[POWERSTEP01_MAX_DEVICES];
uint32_t powerSTEP::_spiClock = 4000000;

uint32_t powerSTEP::_irqSave()
{
#if defined(ARDUINO_ARCH_SAMD)
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
#else
  return 0;
#endif
}

void powerSTEP::_irqRestore(uint32_t primask)
{
#if defined(ARDUINO_ARCH_SAMD)
  if (!primask) __enable_irq();
#else
  (void)primask;
#endif
}

// Constructors
powerSTEP::powerSTEP(int position, int CSPin, int resetPin, int busyPin)
{
  _CSPin = CSPin;
  _position = position;
  _resetPin = resetPin;
  _busyPin = busyPin;
  _SPI = &SPI;
  registerInstance();
}

powerSTEP::powerSTEP(int position, int CSPin, int resetPin)
{
  _CSPin = CSPin;
  _position = position;
  _resetPin = resetPin;
  _busyPin = -1;
  _SPI = &SPI;
  registerInstance();
}

void powerSTEP::registerInstance()
{
  _prepLen = 0;
  _prepBitLen = 0;
  _prepType = PREP_NONE;
  for (byte i = 0; i < 4; i++) { _prepTx[i] = 0; _prepRx[i] = 0; }
  // _numBoards keeps counting past POWERSTEP01_MAX_DEVICES so that SPIXfer() and
  //  performPrepared() can detect an oversized chain.
  if (_numBoards < POWERSTEP01_MAX_DEVICES) _instances[_numBoards] = this;
  _numBoards++;
}

void powerSTEP::SPIPortConnect(SPIClass *SPIPort)
{
  _SPI = SPIPort;
}

int powerSTEP::busyCheck(void)
{
  if (_busyPin == -1)
  {
    if (getParam(REG_STATUS) & 0x0002) return 0;
    else                           return 1;
  }
  else 
  {
    if (digitalRead(_busyPin) == HIGH) return 0;
    else                               return 1;
  }
}
