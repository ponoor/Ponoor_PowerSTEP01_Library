#include "Ponoor_PowerSTEP01Library.h"

//commands.ino - Contains high-level command implementations- movement
//   and configuration commands, for example.

// Command assembly. The same functions are used by the immediate API and by
//  the prepare*() methods, so that clamping and byte order stay identical.

// Writes cmd followed by the low dataBytes bytes of value, most significant
//  byte first (the dSPIN expects big-endian data).
byte powerSTEP::buildData(byte cmd, unsigned long value, byte dataBytes, byte *tx)
{
  tx[0] = cmd;
  for (byte i = 0; i < dataBytes; i++)
  {
    tx[1 + i] = (byte)(value >> ((dataBytes - 1 - i) * 8));
  }
  return 1 + dataBytes;
}

byte powerSTEP::buildRun(byte dir, unsigned long integerSpeed, byte *tx)
{
  if (integerSpeed > 0xFFFFF) integerSpeed = 0xFFFFF;
  return buildData(CMD_RUN | dir, integerSpeed, 3, tx);
}

byte powerSTEP::buildMove(byte dir, unsigned long numSteps, byte *tx)
{
  if (numSteps > 0x3FFFFF) numSteps = 0x3FFFFF;
  return buildData(CMD_MOVE | dir, numSteps, 3, tx);
}

// cmd is CMD_GOTO or CMD_GOTO_DIR | dir.
byte powerSTEP::buildGoTo(byte cmd, long pos, byte *tx)
{
  if (pos > 0x3FFFFF) pos = 0x3FFFFF;
  return buildData(cmd, (unsigned long)pos, 3, tx);
}

// Returns 0 for an unknown register. bitLen receives the register width.
byte powerSTEP::buildSetParam(byte param, unsigned long value, byte *tx, byte *bitLen)
{
  *bitLen = paramBitLen(param);
  if (*bitLen == 0) return 0;
  return buildData(param | CMD_SET_PARAM, paramMask(param, value), (*bitLen + 7) / 8, tx);
}

byte powerSTEP::buildGetParam(byte param, byte *tx, byte *bitLen)
{
  *bitLen = paramBitLen(param);
  if (*bitLen == 0) return 0;
  return buildData(param | CMD_GET_PARAM, 0, (*bitLen + 7) / 8, tx);
}

// Sends a command and its data bytes without letting an interrupt in between.
void powerSTEP::sendBytes(const byte *tx, byte len)
{
  uint32_t primask = _irqSave();
  for (byte i = 0; i < len; i++)
  {
    SPIXfer(tx[i]);
  }
  _irqRestore(primask);
}

// Realize the "set parameter" function, to write to the various registers in
//  the dSPIN chip.
void powerSTEP::setParam(byte param, unsigned long value) 
{
  param |= CMD_SET_PARAM;
  uint32_t primask = _irqSave();
  SPIXfer((byte)param);
  paramHandler(param, value);
  _irqRestore(primask);
}

// Realize the "get parameter" function, to read from the various registers in
//  the dSPIN chip.
long powerSTEP::getParam(byte param)
{
  uint32_t primask = _irqSave();
  SPIXfer(param | CMD_GET_PARAM);
  long retVal = paramHandler(param, 0);
  _irqRestore(primask);
  return retVal;
}

// Returns the content of the ABS_POS register, which is a signed 22-bit number
//  indicating the number of steps the motor has traveled from the HOME
//  position. HOME is defined by zeroing this register, and it is zero on
//  startup.
long powerSTEP::getPos()
{
  long temp = getParam(ABS_POS);
  
  // Since ABS_POS is a 22-bit 2's comp value, we need to check bit 21 and, if
  //  it's set, set all the bits ABOVE 21 in order for the value to maintain
  //  its appropriate sign.
  if (temp & 0x00200000) temp |= ~0x003FFFFFL;
  return temp;
}

// Returns the content of the EL_POS register, which is a 9-bit indicates the current 
//  electrical position of the motor. 
unsigned int powerSTEP::getElPos()
{
  unsigned int temp = getParam(EL_POS);
  return temp;
}

// Just like getPos(), but for MARK.
long powerSTEP::getMark()
{
  long temp = getParam(MARK);
  
  // Since ABS_POS is a 22-bit 2's comp value, we need to check bit 21 and, if
  //  it's set, set all the bits ABOVE 21 in order for the value to maintain
  //  its appropriate sign.
  if (temp & 0x00200000) temp |= ~0x003FFFFFL;
  return temp;
}

// RUN sets the motor spinning in a direction (defined by the constants
//  FWD and REV). Maximum speed and minimum speed are defined
//  by the MAX_SPEED and MIN_SPEED registers; exceeding the FS_SPD value
//  will switch the device into full-step mode.
// The spdCalc() function is provided to convert steps/s values into
//  appropriate integer values for this function.
void powerSTEP::run(byte dir, float stepsPerSec)
{
  unsigned long integerSpeed = spdCalc(stepsPerSec);
  runRaw(dir, integerSpeed);
}
void powerSTEP::runRaw(byte dir, unsigned long integerSpeed) {
  byte tx[4];
  byte len = buildRun(dir, integerSpeed, tx);
  sendBytes(tx, len);
}

// STEP_CLOCK puts the device in external step clocking mode. When active,
//  pin 25, STCK, becomes the step clock for the device, and steps it in
//  the direction (set by the FWD and REV constants) imposed by the call
//  of this function. Motion commands (RUN, MOVE, etc) will cause the device
//  to exit step clocking mode.
void powerSTEP::stepClock(byte dir)
{
  SPIXfer(CMD_STEP_CLOCK | dir);
}

// MOVE will send the motor numStep full steps in the
//  direction imposed by dir (FWD or REV constants may be used). The motor
//  will accelerate according the acceleration and deceleration curves, and
//  will run at MAX_SPEED. Stepping mode will adhere to FS_SPD value, as well.
void powerSTEP::move(byte dir, unsigned long numSteps)
{
  byte tx[4];
  byte len = buildMove(dir, numSteps, tx);
  sendBytes(tx, len);
}

// GOTO operates much like MOVE, except it produces absolute motion instead
//  of relative motion. The motor will be moved to the indicated position
//  in the shortest possible fashion.
void powerSTEP::goTo(long pos)
{
  byte tx[4];
  byte len = buildGoTo(CMD_GOTO, pos, tx);
  sendBytes(tx, len);
}

// Same as GOTO, but with user constrained rotational direction.
void powerSTEP::goToDir(byte dir, long pos)
{
  byte tx[4];
  byte len = buildGoTo(CMD_GOTO_DIR | dir, pos, tx);
  sendBytes(tx, len);
}

// GoUntil will set the motor running with direction dir (REV or
//  FWD) until a falling edge is detected on the SW pin. Depending
//  on bit SW_MODE in CONFIG, either a hard stop or a soft stop is
//  performed at the falling edge, and depending on the value of
//  act (either RESET or COPY) the value in the ABS_POS register is
//  either RESET to 0 or COPY-ed into the MARK register.
void powerSTEP::goUntil(byte action, byte dir, float stepsPerSec)
{
  unsigned long integerSpeed = spdCalc(stepsPerSec);
  goUntilRaw(action, dir, integerSpeed);
}
void powerSTEP::goUntilRaw(byte action, byte dir, unsigned long integerSpeed) {
  action = (action > 0) << 3;
  if (integerSpeed > 0xFFFFF) integerSpeed = 0xFFFFF;  // SPD is 20-bit; the upper 4 bits of byte 2 are don't care
  byte tx[4];
  byte len = buildData(CMD_GO_UNTIL | action | dir, integerSpeed, 3, tx);
  sendBytes(tx, len);
}
// Similar in nature to GoUntil, ReleaseSW produces motion at the
//  higher of two speeds: the value in MIN_SPEED or 5 steps/s.
//  The motor continues to run at this speed until a rising edge
//  is detected on the switch input, then a hard stop is performed
//  and the ABS_POS register is either COPY-ed into MARK or RESET to
//  0, depending on whether RESET or COPY was passed to the function
//  for act.
void powerSTEP::releaseSw(byte action, byte dir)
{
  action = (action > 0) << 3;
  SPIXfer(CMD_RELEASE_SW | action | dir);
}

// GoHome is equivalent to GoTo(0), but requires less time to send.
//  Note that no direction is provided; motion occurs through shortest
//  path. If a direction is required, use GoTo_DIR().
void powerSTEP::goHome()
{
  SPIXfer(CMD_GO_HOME);
}

// GoMark is equivalent to GoTo(MARK), but requires less time to send.
//  Note that no direction is provided; motion occurs through shortest
//  path. If a direction is required, use GoTo_DIR().
void powerSTEP::goMark()
{
  SPIXfer(CMD_GO_MARK);
}

// setMark() and setHome() allow the user to define new MARK or
//  ABS_POS values.
void powerSTEP::setMark(long newMark)
{
  setParam(MARK, newMark);
}

void powerSTEP::setPos(long newPos)
{
  setParam(ABS_POS, newPos);
}

void powerSTEP::setElPos(unsigned int newElPos)
{
  setParam(EL_POS, newElPos);
}


// Sets the ABS_POS register to 0, effectively declaring the current
//  position to be "HOME".
void powerSTEP::resetPos()
{
  SPIXfer(CMD_RESET_POS);
}

// Reset device to power up conditions. Equivalent to toggling the STBY
//  pin or cycling power.
void powerSTEP::resetDev()
{
  SPIXfer(CMD_RESET_DEVICE);
}
  
// Bring the motor to a halt using the deceleration curve.
void powerSTEP::softStop()
{
  SPIXfer(CMD_SOFT_STOP);
}

// Stop the motor with infinite deceleration.
void powerSTEP::hardStop()
{
  SPIXfer(CMD_HARD_STOP);
}

// Decelerate the motor and put the bridges in Hi-Z state.
void powerSTEP::softHiZ()
{
  SPIXfer(CMD_SOFT_HIZ);
}

// Put the bridges in Hi-Z state immediately with no deceleration.
void powerSTEP::hardHiZ()
{
  SPIXfer(CMD_HARD_HIZ);
}

// Fetch and return the 16-bit value in the STATUS register. Resets
//  any warning flags and exits any error states. Using GetParam()
//  to read STATUS does not clear these values.
int powerSTEP::getStatus()
{
  int temp = 0;
  uint32_t primask = _irqSave();
  byte* bytePointer = (byte*)&temp;
  SPIXfer(CMD_GET_STATUS);
  bytePointer[1] = SPIXfer(0);
  bytePointer[0] = SPIXfer(0);
  _irqRestore(primask);
  return temp;
}
