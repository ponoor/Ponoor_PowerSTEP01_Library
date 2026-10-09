#include "Ponoor_PowerSTEP01Library.h"
#include <SPI.h>

// Ponoor_PowerSTEP01LibraryPacked.cpp - Packed daisy-chain transfers.
//
// Each frame of a daisy chain carries one byte for every chip, so the commands
//  of all chips can be sent together: frame 0 holds the command byte of each
//  chip, frame 1 holds the first data byte (or NOP), and so on. A command is
//  at most 4 bytes long, so any combination of commands takes at most 4
//  frames. Chips that have nothing to say in a frame receive 0x00 (NOP).

void powerSTEP::stage(const byte *tx, byte len, byte bitLen, byte type)
{
  if (len > 4) len = 4;
  for (byte i = 0; i < 4; i++)
  {
    _prepTx[i] = (i < len) ? tx[i] : CMD_NOP;
    _prepRx[i] = 0;
  }
  _prepLen = len;
  _prepBitLen = bitLen;
  _prepType = type;
}

void powerSTEP::prepareNop()
{
  stage(NULL, 0, 0, PREP_NONE);
}

// An unknown register cancels the staged command.
void powerSTEP::prepareGetParam(byte param)
{
  byte tx[4], bitLen;
  byte len = buildGetParam(param, tx, &bitLen);
  if (len == 0) { prepareNop(); return; }
  stage(tx, len, bitLen, PREP_GET_PARAM);
}

void powerSTEP::prepareSetParam(byte param, unsigned long value)
{
  byte tx[4], bitLen;
  byte len = buildSetParam(param, value, tx, &bitLen);
  if (len == 0) { prepareNop(); return; }
  stage(tx, len, 0, PREP_SET_PARAM);
}

void powerSTEP::prepareGetStatus()
{
  byte tx[3];
  byte len = buildData(CMD_GET_STATUS, 0, 2, tx);
  stage(tx, len, 16, PREP_GET_STATUS);
}

void powerSTEP::prepareGetPos()
{
  prepareGetParam(ABS_POS);
}

void powerSTEP::prepareRun(byte dir, float stepsPerSec)
{
  prepareRunRaw(dir, spdCalc(stepsPerSec));
}

void powerSTEP::prepareRunRaw(byte dir, unsigned long integerSpeed)
{
  byte tx[4];
  byte len = buildRun(dir, integerSpeed, tx);
  stage(tx, len, 0, PREP_COMMAND);
}

void powerSTEP::prepareMove(byte dir, unsigned long numSteps)
{
  byte tx[4];
  byte len = buildMove(dir, numSteps, tx);
  stage(tx, len, 0, PREP_COMMAND);
}

void powerSTEP::prepareGoTo(long pos)
{
  byte tx[4];
  byte len = buildGoTo(CMD_GOTO, pos, tx);
  stage(tx, len, 0, PREP_COMMAND);
}

void powerSTEP::prepareGoToDir(byte dir, long pos)
{
  byte tx[4];
  byte len = buildGoTo(CMD_GOTO_DIR | dir, pos, tx);
  stage(tx, len, 0, PREP_COMMAND);
}

void powerSTEP::prepareSoftStop()
{
  byte tx[1] = { CMD_SOFT_STOP };
  stage(tx, 1, 0, PREP_COMMAND);
}

void powerSTEP::prepareHardStop()
{
  byte tx[1] = { CMD_HARD_STOP };
  stage(tx, 1, 0, PREP_COMMAND);
}

void powerSTEP::prepareSoftHiZ()
{
  byte tx[1] = { CMD_SOFT_HIZ };
  stage(tx, 1, 0, PREP_COMMAND);
}

void powerSTEP::prepareHardHiZ()
{
  byte tx[1] = { CMD_HARD_HIZ };
  stage(tx, 1, 0, PREP_COMMAND);
}

// Fills packet[] (one byte per chip, indexed by position) for the given frame.
void powerSTEP::assembleFrame(byte frame, byte *packet)
{
  for (int i = 0; i < _numBoards; i++) packet[i] = CMD_NOP;
  for (int i = 0; i < _numBoards; i++)
  {
    powerSTEP *d = _instances[i];
    if (frame < d->_prepLen) packet[d->_position] = d->_prepTx[frame];
  }
}

bool powerSTEP::performPrepared()
{
  const int n = _numBoards;
  if (n <= 0 || n > POWERSTEP01_MAX_DEVICES) return false;

  // All instances must form one chain: same CS pin, same SPI port, and every
  //  position in 0..n-1 used exactly once.
  bool used[POWERSTEP01_MAX_DEVICES];
  for (int i = 0; i < n; i++) used[i] = false;
  byte frames = 0;
  powerSTEP *first = _instances[0];
  for (int i = 0; i < n; i++)
  {
    powerSTEP *d = _instances[i];
    if (d->_CSPin != first->_CSPin || d->_SPI != first->_SPI) return false;
    if (d->_position < 0 || d->_position >= n || used[d->_position]) return false;
    used[d->_position] = true;
    if (d->_prepLen > frames) frames = d->_prepLen;
  }
  if (frames == 0) return true;  // nothing staged

  byte packet[POWERSTEP01_MAX_DEVICES];
  SPIClass *spi = first->_SPI;
  uint32_t primask = _irqSave();
  for (byte f = 0; f < frames; f++)
  {
    assembleFrame(f, packet);
    // CS stays HIGH between frames for at least the tdisCS (625 ns) required
    //  by the datasheet: it covers endTransaction(), digitalWrite() HIGH/LOW
    //  and beginTransaction(), which take several microseconds on SAMD.
    digitalWrite(first->_CSPin, LOW);
    spi->beginTransaction(SPISettings(_spiClock, MSBFIRST, SPI_MODE3));
    spi->transfer(packet, n);
    spi->endTransaction();
    digitalWrite(first->_CSPin, HIGH);
    for (int i = 0; i < n; i++)
    {
      powerSTEP *d = _instances[i];
      if (f < d->_prepLen) d->_prepRx[f] = packet[d->_position];
    }
  }
  _irqRestore(primask);

  for (int i = 0; i < n; i++) _instances[i]->_prepLen = 0;
  return true;
}

// Assembles the response bytes (the first byte is the reply to the command
//  byte and carries no data) and masks them to the register width.
unsigned long powerSTEP::preparedResult()
{
  if (_prepBitLen == 0) return 0;
  byte byteLen = (_prepBitLen + 7) / 8;
  unsigned long value = 0;
  for (byte i = 1; i <= byteLen && i < 4; i++)
  {
    value = (value << 8) | _prepRx[i];
  }
  return value & (0xFFFFFFFFUL >> (32 - _prepBitLen));
}

long powerSTEP::preparedPos()
{
  long temp = (long)preparedResult();
  if (temp & 0x00200000) temp |= ~0x003FFFFFL;
  return temp;
}

int powerSTEP::preparedStatus()
{
  return (int)preparedResult();
}
