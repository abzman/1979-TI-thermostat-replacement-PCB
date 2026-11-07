// Simulates the schematic's gate network and checks levels_for() against it.
#include <stdio.h>
#include "board_io.h"
using board::Outputs;
struct Triacs { bool heat, cool, fan, humid; };
// 74HC00 stages exactly as drawn; optotriac LEDs are active-low (anode on +3V3).
static Triacs hardware(int io4, int io19, int io2, int io27) {
  auto nand = [](int a, int b) { return !(a && b); };
  int u7d = nand(io19, io19), u7c = nand(u7d, u7d);          // inverter, inverter
  int heat_n = nand(u7c, io4), cool_n = nand(u7d, io4);
  int fan_n = nand(nand(io2, io2), 1), humid_n = nand(nand(io27, io27), 1);
  return {!heat_n, !cool_n, !fan_n, !humid_n};
}
int main() {
  int bad = 0, both = 0;
  for (int m = 0; m < 16; m++) {
    Outputs o; o.heat = m & 1; o.cool = m & 2; o.fan = m & 4; o.humid = m & 8;
    auto p = board::levels_for(o);
    Triacs t = hardware(p.enable, p.heat_select, p.fan_pin, p.humid_pin);
    bool eh = o.heat && !o.cool, ec = o.cool && !o.heat;      // both requested -> both off
    if (t.heat != eh || t.cool != ec || t.fan != o.fan || t.humid != o.humid) { bad++; printf("MISMATCH req=%x\n", m); }
  }
  for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++)      // hardware lockout for ANY IO4/IO19 state
    for (int c = 0; c < 2; c++) for (int d = 0; d < 2; d++) { auto t = hardware(a, b, c, d); if (t.heat && t.cool) both++; }
  printf("request->triac check: %s\n", bad ? "FAIL" : "all 16 request combinations produce the intended triac states");
  printf("heat+cool simultaneously possible for any of the 16 pin states: %s\n", both ? "YES (bad)" : "no");
  Triacs rst = hardware(0, 0, 0, 1);
  printf("pins IO4=0 IO19=0 IO2=0 IO27=1 (what a reset with IO2 pulled low looks like): fan %s, humid %s\n", rst.fan ? "ON" : "off", rst.humid ? "ON" : "off");
}
