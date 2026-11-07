#include <stdio.h>
#include "switches.h"
int main() {
  int bad = 0;
  struct { int a, b; sw::Mode m; uint8_t k; const char *n; } mode[] = {
    {1,1,sw::Mode::Constant,0x04,"Constant"}, {0,1,sw::Mode::DayNight,0x00,"Day/Night"}, {1,0,sw::Mode::Night,0x02,"Night"}};
  struct { int a, b; sw::System s; uint8_t k; const char *n; } sys[] = {
    {1,1,sw::System::Heat,0x04,"Heat"}, {0,1,sw::System::Off,0x00,"Off"}, {1,0,sw::System::Cool,0x02,"Cool"}};
  for (auto &m : mode) for (auto &s : sys) {
    sw::Switches r = sw::decode(m.a, m.b, s.a, s.b);   // io36,io39 = mode pins ; io34,io35 = system pins
    bool ok = r.mode == m.m && r.system == s.s && sw::rom_k_for_mode_row(r.mode) == m.k && sw::rom_k_for_system_row(r.system) == s.k;
    if (!ok) { bad++; printf("FAIL %s / %s\n", m.n, s.n); }
  }
  printf("9 mode x system combinations decode and map to ROM K bits: %s\n", bad ? "FAIL" : "ok");
  // debounce: Day/Night -> (open) -> Night must not flash 'Constant'
  sw::Debounce d(5); sw::Switches raw;
  for (int i = 0; i < 6; i++) d.update(sw::decode(0,1,1,1));           // settle on Day/Night
  bool flashed = false;
  d.update(sw::decode(1,1,1,1)); if (d.stable().mode == sw::Mode::Constant) flashed = true;   // 1 scan in the open gap
  d.update(sw::decode(1,1,1,1)); if (d.stable().mode == sw::Mode::Constant) flashed = true;
  for (int i = 0; i < 6; i++) d.update(sw::decode(1,0,1,1));           // arrives at Night
  printf("debounce: gap does not flash Constant: %s, ends on Night: %s\n", flashed ? "NO" : "yes", d.stable().mode == sw::Mode::Night ? "yes" : "NO");
}
