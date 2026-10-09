Ponoor PowerSTEP01 Library
==========================

Arduino library for the STMicroelectronics [powerSTEP01](https://www.st.com/en/motor-drivers/powerstep01.html) stepper motor driver, with voltage mode and current mode drive.

This library is a modification of the Megunolink [powerSTEP01_Arduino_Library](https://github.com/Megunolink/powerSTEP01_Arduino_Library), which is a modification of the L6470-based SparkFun [AutoDriver library](https://github.com/sparkfun/SparkFun_AutoDriver_Arduino_Library). It is used in Ponoor products such as the STEP400 stepper motor driver board, and also works with the STMicroelectronics [X-NUCLEO-IHM03A1](https://www.st.com/en/ecosystems/x-nucleo-ihm03a1.html) shield. It runs on SAMD (ARM Cortex-M0+) boards as well as other Arduino architectures.

The sister library for the L6470 is [Ponoor_L6470_Library](https://github.com/ponoor/Ponoor_L6470_Library).

Repository Contents
-------------------
* **src** - Source of the Arduino library.
* **examples** - Example sketches demonstrating the use of the library.
* **test** - Host-side tests (not part of the Arduino build). See [Running the tests](#running-the-tests).
* **keywords.txt** - List of words to be highlighted by the Arduino IDE.
* **library.properties** - Used by the Arduino package manager.
* **CHANGELOG.md** - Release history.

Install
-------
This library can be installed from the Library Manager of the Arduino IDE.

Daisy chain
-----------
Several powerSTEP01 chips can be connected in a daisy chain that shares one CS pin. Every `powerSTEP` instance represents one chip:

```cpp
powerSTEP board0(0, 10, 8);  // position 0, CS pin 10, reset pin 8
powerSTEP board1(1, 10, 8);  // position 1, same CS pin
```

The first argument, `position`, is the position of the chip in the chain, counted from 0 at the chip **farthest** from the controller (the end of the chain). All instances in the chain must share the same CS pin, and every instance must be constructed before the first transfer, because the library needs to know the length of the chain.

The maximum number of instances is `POWERSTEP01_MAX_DEVICES` (default 16). Define it before including the library to change it. A longer chain is not supported: the regular API sends nothing and `performPrepared()` returns `false`.

`powerSTEP::setSPIClock(hz)` changes the SPI clock (default 4 MHz, clamped to the datasheet maximum of 5 MHz).

### Packed transfers

With the regular API, every call shifts the whole chain: each byte of a command is sent as a frame of N bytes (the byte for the target chip and NOP for all the others). Sending one command to each of N chips therefore costs N x N bytes per byte of command.

A frame of a daisy chain can carry a different byte for every chip, so the commands of all chips can be sent together. The `prepare*()` methods stage a command in an instance without any SPI traffic. `powerSTEP::performPrepared()` then sends the staged commands of all instances in at most 4 frames (the maximum length of a command), and the responses are read back with `preparedPos()`, `preparedStatus()` or `preparedResult()`.

```cpp
// Read the position of every motor with one transfer...
for (int i = 0; i < NUM_BOARDS; i++) boards[i]->prepareGetPos();
powerSTEP::performPrepared();

// ...and update the speed of every motor with another one.
for (int i = 0; i < NUM_BOARDS; i++) {
  long pos = boards[i]->preparedPos();
  boards[i]->prepareRun(pos < target ? FWD : REV, 500);
}
powerSTEP::performPrepared();
```

Available methods: `prepareGetParam()`, `prepareSetParam()`, `prepareGetStatus()`, `prepareGetPos()`, `prepareRun()`, `prepareRunRaw()`, `prepareMove()`, `prepareGoTo()`, `prepareGoToDir()`, `prepareSoftStop()`, `prepareHardStop()`, `prepareSoftHiZ()`, `prepareHardHiZ()` and `prepareNop()` (cancels the staged command).

Notes and restrictions:

* Only one command can be staged per instance; preparing again overwrites it. Instances without a staged command receive NOP.
* The results stay valid until the next `prepare*()` call on the same instance.
* Packed transfers require a **single chain**: all instances must share the same CS pin and the same SPI port. Otherwise (or if positions are duplicated or out of range) `performPrepared()` sends nothing and returns `false`.
* `prepareSetParam()` writes the value as is, because nothing can be read between the commands of a packed transfer. Registers that contain a flag next to a value are therefore overwritten as a whole:
  * Writing `FS_SPD` clears `BOOST_MODE` (bit 10) unless the value includes it.
  * Writing `MIN_SPEED` clears `LSPD_OPT` (bit 12) unless the value includes it.

  `setFullSpeedRaw()`, `setMinSpeedRaw()`, `setBoostMode()` and `setLoSpdOpt()` keep the flags, because they read the register first. Use them (or include the flag in the value) when the flag is in use.

Slew rate
---------
`setSlewRate()` takes one of the constants below. The value is written to bits 7:0 of `GATECFG1`: `IGATE` (bits 7:5) and `TCC` (bits 4:0). The other bits of the register (`TBOOST` and `WD_EN`) are not changed. The values are those of Table 11 of the datasheet (VS = 48 V) and `getSlewRate()` returns the same value.

| Constant | Slew rate | IGATE | tCC | Value | Recommended TDT | Recommended TBLANK |
| --- | --- | --- | --- | --- | --- | --- |
| `SR_114V_us` | 114 V/us | 8 mA | 3125 ns | `0x58` | 125 ns | 250 ns |
| `SR_220V_us` | 220 V/us | 16 mA | 1625 ns (*) | `0x6C` | 125 ns | 250 ns |
| `SR_400V_us` | 400 V/us | 24 mA | 1000 ns | `0x87` | 125 ns | 250 ns |
| `SR_520V_us` | 520 V/us | 32 mA | 875 ns | `0xA6` | 125 ns | 250 ns |
| `SR_790V_us` | 790 V/us | 64 mA | 500 ns | `0xC3` | 125 ns | 375 ns |
| `SR_980V_us` | 980 V/us | 96 mA | 375 ns | `0xE2` | 125 ns | 500 ns |

(*) Table 11 gives 1600 ns, but `TCC` has a step of 125 ns, so 1625 ns is the closest value.

`setSlewRate()` only sets `GATECFG1`. Table 11 also recommends a dead time (`TDT`) and a blanking time (`TBLANK`) in `GATECFG2`, which are **not** set by the library. `TDT` is bits 4:0 and `TBLANK` is bits 7:5, both in units of 125 ns minus one. If you need the recommended values, set them yourself:

```cpp
driver.setSlewRate(SR_980V_us);
// TDT = 0 (125 ns), TBLANK = 3 (500 ns): GATECFG2 = TBLANK << 5 | TDT
driver.setParam(GATECFG2, (3 << 5) | 0);
```

A faster slew rate reduces the switching losses but increases EMI. Check the temperature of the driver and the EMI on your board when you change the setting.

Notes
-----

### Registers
Since the registers shown in the table below are physically identical, for example, when you change `KVAL_HOLD`, the value of `TVAL_HOLD` is also changed. Please save them in your Arduino sketch when you switch the control mode, if necessary.

| Address | Register name in Voltage mode | Register name in Current mode |
| --- | --- | --- |
| h09 | KVAL_HOLD | TVAL_HOLD |
| h0A | KVAL_RUN | TVAL_RUN |
| h0B | KVAL_ACC | TVAL_ACC |
| h0C | KVAL_DEC | TVAL_DEC |
| h0E | ST_SLP | T_FAST |
| h0F | FN_SLP_ACC | TON_MIN |
| h10 | FN_SLP_DEC | TOFF_MIN |

The `TVAL_*` registers are 7 bits wide; the `set*TVAL()` methods mask the value to 7 bits.

`FS_SPD` contains the `BOOST_MODE` flag in bit 10. `setFullSpeed()` and `setFullSpeedRaw()` keep the flag, and `setBoostMode()` / `getBoostMode()` change and read it.

### SPI for X-NUCLEO-IHM03A1
X-NUCLEO-IHM03A1 has an Arduino form factor but the SPI pins appear on pins D11-D13, instead of the SPI socket. This is the classic SPI pinout used in Arduino UNO or earlier models, so this shield won't work with other Arduinos like Leonardo/Mega. In Arduino Zero/M0, you can use these pins by configuring them to behave as SPI pins.

Differences from the original library
-------------------------------------
- Added the current mode functions (`setCurrentMode()`, `setVoltageMode()`, `set/get*TVAL()`, `setPredictiveControl()`, `setSwitchingPeriod()`).
- Added `getSpeed()`.
- Added raw register accessors (`set/get*Raw()`, `runRaw()`, `goUntilRaw()`).
- Added `getElPos()` / `setElPos()`.
- Added `setBoostMode()` / `getBoostMode()`. `FS_SPD` is treated as an 11-bit register, and `setFullSpeedRaw()` keeps `BOOST_MODE`.
- Added packed daisy-chain transfers (`prepare*()` / `performPrepared()`) and `setSPIClock()`.
- Fixed `SR_980V_us` (96 mA / 375 ns). It used to be written as 64 mA / 2375 ns. **The real switching speed with this setting is now faster than before.**
- Fixed `getSlewRate()`, which read `CONFIG` instead of `GATECFG1`.
- Fixed unit conversion factors for ACC, DEC, INT_SPD, and the RUN speed according to the datasheet. ACC is clamped to 0x001-0xFFE (0xFFF is reserved), DEC to 0x001-0xFFF and MAX_SPEED to at least 1 (0 is reserved). The `setAcc()` / `setDec()` methods round to the nearest register value.
- Fixed the register widths of `ADC_OUT` (5 bits) and `GATECFG1`, and the values written by the `set*TVAL()` methods (7 bits).
- `CURRENT_MODE` is now `0x08`, the value of the `CM_VM` bit. Added `CONFIG_EN_TQREG`.
- Fixed `ACT` bit of `goUntil()` and `releaseSw()`: any non-zero `action` is treated as COPY.
- `goUntilRaw()` clamps the speed to 20 bits.
- Fixed `STATUS` register bit names and bit masks, and the mask of `CONFIG_UVLOVAL_HIGH`.
- Interrupts are disabled during SPI transactions on SAMD to prevent corrupted return values.
- Renamed command and status constants (`CMD_*`, `REG_STATUS`) to avoid conflicts with other libraries. The old command names are available when `POWERSTEP01_LEGACY_COMMAND_NAMES` is defined before including the library.
- Unique include guards.

See [CHANGELOG.md](CHANGELOG.md) for details.

Examples
--------
* **powerSTEP01SimpleTest** - Basic example for the X-NUCLEO-IHM03A1 on an Arduino Uno-compatible board (voltage mode).
* **powerSTEP01SimpleTest_zero** - The same for an Arduino Zero-compatible board, using a SERCOM SPI port.
* **currentMode** - Current mode example for an Arduino Uno-compatible board.
* **currentMode_zero** - Current mode example for an Arduino Zero-compatible board.
* **MegunolinkDriverInterface_powerSTEP01_** - Controls a driver from the MegunoLink software. It requires the [MegunoLink Arduino library](https://www.megunolink.com/) (`MegunoLink.h`, `CommandHandler.h` and `EEPROMStore.h`) to be installed.
* **PackedBasics** - Minimal packed-transfer example for 4 boards: read positions/status and send a different command to each board, controlled from the Serial Monitor.
* **PackedCommands** - Compares the regular and the packed API on a 4-board chain and runs a simple P-control servo loop. The number of boards is set by `NUM_BOARDS`.

The `_zero` examples use the SERCOM SPI constructor of older Arduino SAMD core versions; with the current `arduino:samd` core (1.8.x) `SPIClass` has to be replaced with `SPIClassSAMD` and `setDataMode()` removed.

Running the tests
-----------------
The tests run on the host with mocked Arduino and SPI headers:

```
g++ -std=c++11 -Itest/mock -Itest -Isrc test/conversion_test.cpp src/*.cpp -o conversion_test && ./conversion_test
g++ -std=c++11 -Itest/mock -Itest -Isrc test/packed_test.cpp src/*.cpp -o packed_test && ./packed_test
g++ -std=c++11 -DPOWERSTEP01_MAX_DEVICES=2 -Itest/mock -Itest -Isrc test/limits_test.cpp src/*.cpp -o limits_test && ./limits_test
```

License Information
-------------------
This product is open source!

The code is beerware; if you see any SparkFun employee at the local, and you've found their code helpful, please buy them a round!

Please use, reuse, and modify these files as you see fit. Please maintain attribution to SparkFun Electronics and release anything derivative under the same license.

Distributed as-is; no warranty is given.
