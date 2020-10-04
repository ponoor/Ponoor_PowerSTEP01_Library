Ponoor PowerSTEP01 Library
==========
Arduino library support for STMicroelectronics PowerSTEP01 stepper driver chip with voltage and current mode drive.

This library is a modification of Megunolink powerSTEP01_Arduino_Library which is a modification of the L6470-based SparkFun AutoDriver library.

Differences from the original library
-------------------
- Added the current control drive mode functions
- Added getSpeed() function
- Disable USB interrupts during getStatus() to avoid return value collapse for ATSAMD
- Fixed ACT bit of goUntil and releaseSw
- Fixed STATUS register bit names and bit masks.

Repository Contents
-------------------
* **src** - Contains the source for the Arduino library.
* **Examples** - Example sketches demonstrating the use of the library
* **keywords.txt** - List of words to be highlighted by the Arduino IDE
* **library.properties** - Used by the Arduino package manager

Documentation
-------------------
* **[Installing an Arduino Library Guide](https://learn.sparkfun.com/tutorials/installing-an-arduino-library)** - Basic information on how to install an Arduino library.

License Information
-------------------
This product is open source!
The code is beerware; if you see any SparkFun employee at the local, and you've found their code helpful, please buy them a round!
Please use, reuse, and modify these files as you see fit. Please maintain attribution to SparkFun Electronics and release anything derivative under the same license.

Distributed as-is; no warranty is given.
