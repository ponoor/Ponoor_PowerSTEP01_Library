Ponoor PowerSTEP01 Library
==========
Arduino library support for STMicroelectronics [PowerSTEP01 stepper driver chip](https://www.st.com/en/motor-drivers/powerstep01.html) with voltage and current mode drive.

This library is a modification of Megunolink [powerSTEP01_Arduino_Library](https://github.com/Megunolink/powerSTEP01_Arduino_Library) which is a modification of the L6470-based SparkFun [AutoDriver library](https://github.com/sparkfun/SparkFun_AutoDriver_Arduino_Library).

Differences from the original library
-------------------
- Added the current control drive mode functions
- Added `getSpeed()` function
- Disable USB interrupts during `getStatus` and `xferParam` to avoid return value collapse for ATSAMD
- Fixed `ACT` bit of goUntil and releaseSw
- Fixed `STATUS` register bit names and bit masks.
- Change some constants name(`GET_STATUS`, `STATUS`) to avoid conflicts with other libraries.

Notes
-------------------
Since the registers shown in the table below are physically identical, for example, when you change `KVAL_HOLD`, the value of `TVAL_HOLD` is also changed. Please save them in Arduino sketch when you switch the control mode if necessary.

| Address | Register name in Voltage mode | Register name in Current mode |
| --- | --- | --- |
| h09 | `KVAL_HOLD` | `TVAL_HOLD` |
| h0A | `KVAL_RUN` | `TVAL_RUN` |
| h0B | `KVAL_ACC` | `TVAL_ACC` |
| h0C | `KVAL_DEC` | `TVAL_DEC` |
| h0E | `ST_SLP` | `T_FAST` |
| h0F | `FN_SLP_ACC` | `TON_MIN` |
| h10 | `FN_SLP_DEC` | `TOFF_MIN` |

Install
-------------------
This library can be installed from the library maneger of Arduino IDE.

Repository Contents
-------------------
* **src** - Contains the source for the Arduino library.
* **Examples** - Example sketches demonstrating the use of the library
* **keywords.txt** - List of words to be highlighted by the Arduino IDE
* **library.properties** - Used by the Arduino package manager

License Information
-------------------
This product is open source!
The code is beerware; if you see any SparkFun employee at the local, and you've found their code helpful, please buy them a round!
Please use, reuse, and modify these files as you see fit. Please maintain attribution to SparkFun Electronics and release anything derivative under the same license.

Distributed as-is; no warranty is given.
