// Minimal powerSTEP01 daisy-chain simulator for host-side tests. It models the
// SPI byte protocol (command byte, 1-3 argument bytes, response bytes) and a
// register file with the byte lengths of datasheet Table 12.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Arduino.h"

struct Chip {
  uint32_t regs[0x20] = {0};
  std::vector<std::string> log;  // every completed write / command
  byte cmd = 0;
  int remaining = 0;
  int replyIdx = 0;
  uint32_t acc = 0;
  byte reply[4] = {0, 0, 0, 0};

  // Number of data bytes of a register (0 = no such register).
  static int regBytes(byte addr) {
    switch (addr) {
      case 0x01: case 0x03: case 0x04: return 3;
      case 0x02: case 0x05: case 0x06: case 0x07: case 0x08:
      case 0x0D: case 0x15: case 0x18: case 0x1A: case 0x1B: return 2;
      case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0E: case 0x0F:
      case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x16:
      case 0x17: case 0x19: return 1;
      default: return 0;
    }
  }
  // Data bytes following a command byte.
  static int dataBytes(byte c) {
    if (c == 0xD0) return 2;                                  // GetStatus
    if ((c & 0xFE) == 0x50 || (c & 0xFE) == 0x40 ||
        (c & 0xFE) == 0x60 || (c & 0xFE) == 0x68) return 3;   // Run/Move/GoTo/GoTo_DIR
    if ((c & 0xFC) == 0x80 && (c & 0x02)) return 3;           // GoUntil
    if ((c & 0xE0) == 0x20 || ((c & 0xE0) == 0x00 && c != 0)) return regBytes(c & 0x1F);
    return 0;
  }

  byte xfer(byte in) {
    if (remaining == 0) {
      cmd = in; acc = 0; replyIdx = 0;
      remaining = dataBytes(in);
      for (int i = 0; i < 4; i++) reply[i] = 0;
      if ((in & 0xE0) == 0x20) {
        uint32_t v = regs[in & 0x1F];
        for (int i = 0; i < remaining; i++) reply[i] = (v >> ((remaining - 1 - i) * 8)) & 0xFF;
      } else if (in == 0xD0) {
        uint32_t v = regs[0x1B];
        for (int i = 0; i < 2; i++) reply[i] = (v >> ((1 - i) * 8)) & 0xFF;
      }
      if (remaining == 0 && in != 0) log.push_back("cmd " + std::to_string(in));
      return 0;  // response to the command byte
    }
    acc = (acc << 8) | in;
    byte out = reply[replyIdx++];
    if (--remaining == 0) {
      if ((cmd & 0xE0) == 0x00) {
        regs[cmd & 0x1F] = acc;
        log.push_back("set " + std::to_string(cmd) + "=" + std::to_string(acc));
      } else if ((cmd & 0xE0) != 0x20 && cmd != 0xD0) {
        log.push_back("cmd " + std::to_string(cmd) + " " + std::to_string(acc));
      }
    }
    return out;
  }
};
