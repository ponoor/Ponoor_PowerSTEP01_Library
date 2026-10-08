# Changelog

## v1.2.0 (Unreleased)

### Behavior changes
- `SR_980V_us` changed from `0xD2` to `0xE2` (IGATE 96 mA, tCC 375 ns, as in datasheet Table 11). v1.1.0 wrote 64 mA / 2375 ns by mistake. The STEP400 firmware uses `SR_980V_us` by default, so the real switching becomes faster. **Check the driver temperature and EMI on real hardware before releasing firmware that uses it.** The other five `SR_*` values are unchanged.
- `getAcc()` / `getDec()` now return values about 4.9% smaller than v1.1.0. v1.1.0 used the MAX_SPEED / FS_SPD factor (15.258789) by mistake; the new factor (14.551915 steps/s/s per LSB) matches the datasheet.
- `setAcc()` / `setDec()` round to the nearest register value instead of truncating, so the written value may differ by 1 LSB (e.g. `setAcc(1000)` writes 69 instead of 68). `setAcc()` clamps to 0x001-0xFFE (0xFFF is reserved) and `setDec()` to 0x001-0xFFF (0xFFF is valid for DEC on the powerSTEP01, unlike the L6470).
- `setMaxSpeed()` writes at least 1; 0x000 is reserved by the datasheet and was written for `setMaxSpeed(0)` before.
- `getSlewRate()` returns `GATECFG1` bits 7:0, i.e. the value passed to `setSlewRate()`. It used to return `CONFIG` bits 9:8 (`VCCVAL` / `UVLOVAL`).
- `getFullSpeedRaw()` returns only the lower 10 bits of `FS_SPD`; `BOOST_MODE` (bit 10) is not included. `setFullSpeedRaw()` keeps `BOOST_MODE` and writes only the lower 10 bits.
- `CURRENT_MODE` is now `0x08` (the `CM_VM` bit of `STEP_MODE`); it was `0x01`. The library does not use this constant.
- The command constants were renamed with the `CMD_` prefix: `NOP`, `SET_PARAM`, `GET_PARAM`, `RUN`, `STEP_CLOCK`, `MOVE`, `GOTO`, `GOTO_DIR`, `GO_UNTIL`, `RELEASE_SW`, `GO_HOME`, `GO_MARK`, `RESET_POS`, `RESET_DEVICE`, `SOFT_STOP`, `HARD_STOP`, `SOFT_HIZ` and `HARD_HIZ` are now `CMD_NOP`, `CMD_SET_PARAM`, and so on. To keep a sketch that uses the old names compiling, define `POWERSTEP01_LEGACY_COMMAND_NAMES` before including the library (e.g. `#define POWERSTEP01_LEGACY_COMMAND_NAMES` at the top of the sketch, or `-DPOWERSTEP01_LEGACY_COMMAND_NAMES`). Otherwise replace the names with the `CMD_` ones.
- A chain longer than `POWERSTEP01_MAX_DEVICES` (default 16) is no longer supported; `SPIXfer()` does nothing in that case. Define `POWERSTEP01_MAX_DEVICES` before including the library to raise the limit.
- The `set*TVAL()` methods mask the value to 7 bits (`TVAL_*` are 7-bit registers).

### Fixed
- `SR_980V_us`: wrong IGATE value (see Behavior changes). Comment of its tCC (375 ns) corrected.
- `getSlewRate()` read the wrong register.
- `FS_SPD` is 11 bits wide (bit 10 is `BOOST_MODE`). `setFullSpeedRaw()` no longer clears `BOOST_MODE`.
- `ADC_OUT` is 5 bits wide.
- ACC/DEC read-back factor (1 LSB = 14.551915 steps/s/s). `getDec()` now uses `decParse()`.
- `setAcc()` no longer writes the reserved value 0xFFF.
- `goUntilRaw()` clamps the speed to 20 bits (0xFFFFF) like `runRaw()`. Previously values from 0x100000 to 0x3FFFFF were sent as is; the chip ignored the upper bits, so a too-large speed could result in a very low speed.
- On SAMD, interrupts are disabled for the whole SPI transaction (command and data bytes) of `setParam()`, `getParam()`, `getStatus()` and multi-byte motion commands. PRIMASK is saved and restored, so the protection nests safely.
- Include guards are unique (`PONOOR_POWERSTEP01_CONSTANTS_H`, `PONOOR_POWERSTEP01_LIBRARY_H`, `PONOOR_POWERSTEP01_CONFIGURATION_STRUCTURES_H`). The old constants guard (`_dspin_constants_h_`) clashed with Ponoor_L6470_Library, and the configuration structures header had none.
- Sign extension in `getPos()` / `getMark()` no longer depends on the size of `long`.
- The Megunolink example included the old file names `powerSTEP01ConfigurationStructures.h` and `powerSTEP01ArduinoLibrary.h` and did not compile.
- Stale comments: conversion factors, the removed "infinite acceleration" mode, `OCD_TH` / `STALL_TH` (these are voltage thresholds in 31.25 mV steps, not currents), `F_PWM_INT` / `F_PWM_DEC` in `CONFIG`.

### Added
- `setBoostMode()` / `getBoostMode()`.
- `CONFIG_EN_TQREG` (current mode `EN_TQREG`, the same bit as `CONFIG_EN_VSCOMP`) and `FS_SPD_BOOST_MODE`.
- Packed daisy-chain transfers: `prepare*()` methods, `powerSTEP::performPrepared()` (returns `false` and sends nothing if the prepared devices do not share the same CS pin and SPI port), `preparedResult()`, `preparedPos()` and `preparedStatus()`. `prepareSetParam()` cannot keep `BOOST_MODE` (`FS_SPD`) and `LSPD_OPT` (`MIN_SPEED`) because nothing can be read between the commands.
- `powerSTEP::setSPIClock()` (default 4 MHz, clamped to 5 MHz).
- `POWERSTEP01_MAX_DEVICES` and `POWERSTEP01_LEGACY_COMMAND_NAMES` options.
- `PackedBasics` and `PackedCommands` examples (4 boards).
- Host-side tests in `test/`.

### Changed
- `SPIXfer()` uses a fixed-size buffer instead of a variable-length array.
- Command assembly and the register width table are shared between the immediate and packed APIs.
- The tdisCS in comments is 625 ns (powerSTEP01), not 800 ns.
- Source files use LF line endings and 2-space indentation.
- `keywords.txt` updated; README rewritten (slew rate table, daisy chain, packed transfers); `library.properties` description updated.
- The MegunoLink example documents the libraries it depends on.

### Known issues
- `setSlewRate()` sets `GATECFG1` only. The recommended `TDT` / `TBLANK` values of datasheet Table 11 (`GATECFG2`) have to be set with `setParam(GATECFG2, ...)`; see the README.
- Datasheet Table 12 gives the length of `GATECFG1` as 11 bits, but Table 33 places `WD_EN` at bit 11, so the library treats the register as 12 bits wide (mask `0x0FFF`).
- The `_zero` examples do not compile with the `arduino:samd` core 1.8.x (`SPIClass` constructor and `setDataMode()` changed in the core).

## v1.1.0 (2024-09-20)
- Fixed unit conversion factors according to the datasheet:
  - ACC/DEC: written values were twice the correct value (factor 0.137438 -> 0.06871948).
  - INT_SPD: written values were a quarter of the correct value (factor 4.1943 -> 16.777216).
  - MIN_SPEED and RUN speed: small corrections (about 0.2% and 0.004%).
  - MAX_SPEED and FS_SPD: only the notation changed.
- `setAcc()` clamps to 0xFFE because 0xFFF is reserved.
- Known issue: `getAcc()` / `getDec()` used the factor of MAX_SPEED (15.258789) instead of 14.551915 and returned values about 4.9% too large (fixed in v1.2.0).

## v1.0.2 (2021-06-27)
- Updated `library.properties`.

## v1.0.1 (2021-04-30)
- Added `getElPos()` / `setElPos()`.
- Note: the `library.properties` of the v1.0.1 tag still says 1.0.0; it was changed to 1.0.1 and then 1.0.2 on 2021-06-27.

## v1.0.0 (2020-10-07)
- Forked from the Megunolink powerSTEP01 Arduino Library.
- Added the current mode: `setCurrentMode()` / `setVoltageMode()`, `set/get*TVAL()`, `setPredictiveControl()`, `setSwitchingPeriod()`, the current mode registers (`TVAL_*`, `T_FAST`, `TON_MIN`, `TOFF_MIN`) and `CONFIG_PRED` / `CONFIG_TSW`.
- Added `getSpeed()` and raw register accessors (`set/get*Raw()`, `runRaw()`, `goUntilRaw()`).
- Disabled interrupts during `getStatus()` and `xferParam()` on SAMD.
- Renamed the STATUS register constant to `REG_STATUS` and the status command to `CMD_GET_STATUS` to avoid conflicts.
- Fixed `STATUS_*` and `ALARM_EN_*` bit names and bit masks, including the shift of `STATUS_MOT_STATUS_*`.
- Fixed the mask of `CONFIG_UVLOVAL_HIGH`.
- `goUntil()` / `releaseSw()` treat any non-zero `action` as COPY.
- Added the `currentMode`, `currentMode_zero` and `powerSTEP01SimpleTest_zero` examples.
