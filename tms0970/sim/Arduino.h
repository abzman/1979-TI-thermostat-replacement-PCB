// Minimal Arduino stub + simulated key matrix, just enough to run keymap_probe.ino on the host.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define INPUT 0
#define OUTPUT 1
extern uint64_t sim_now_us;
extern int sim_addr_pin[3];
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t pin, uint8_t v);
int digitalRead(uint8_t pin);
inline void delayMicroseconds(unsigned us) { sim_now_us += us; }
inline void delay(unsigned ms) { sim_now_us += (uint64_t)ms * 1000; }
struct SerialStub {
  void begin(int) {}
  void print(const char *s) { fputs(s, stdout); }
  void print(int v) { printf("%d", v); }
  void println() { putchar('\n'); }
  void println(const char *s) { puts(s); }
  void println(int v) { printf("%d\n", v); }
  int available() { return 0; }
  int read() { return -1; }
};
extern SerialStub Serial;
