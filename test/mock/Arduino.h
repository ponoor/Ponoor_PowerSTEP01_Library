// Minimal Arduino core mock for host-side tests.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define MSBFIRST 1

void digitalWrite(int pin, int value);
int digitalRead(int pin);
