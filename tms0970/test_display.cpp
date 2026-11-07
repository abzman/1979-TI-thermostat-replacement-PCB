// Host check: (1) ROM 7-seg font maps onto the standard MAX7219 font, (2) end-to-end:
// set the clock to 12:30 on the emulated ROM and look at the MAX7219 register writes.
#include <stdio.h>
#include <string.h>
#include "tms0970.h"
#include "display_integrator.h"
#include "max7219_display.h"
#include "tithermos_tables.h"

struct FakeBus { uint8_t reg[16] = {}; int writes = 0;
  void write(uint8_t r, uint8_t v) { reg[r] = v; writes++; } };

struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0;
  uint8_t read_k() override { uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; } return k; }
  void write_o(uint8_t v) override { o = v; } void write_r(uint16_t v) override { r = v; } };

int main() {
  // (1) font check against the standard MAX7219 raw-segment font (0..9)
  const uint8_t rom_font[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
  const uint8_t max_font[10] = {0x7E,0x30,0x6D,0x79,0x33,0x5B,0x5F,0x70,0x7F,0x7B};
  int bad = 0;
  for (int i = 0; i < 10; i++) if (max7219::from_rom_segments(rom_font[i]) != max_font[i]) { bad++; printf("font mismatch %d\n", i); }
  printf("font mapping: %s\n", bad ? "FAIL" : "all 10 digits match the standard MAX7219 font");

  // (2) end-to-end
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b); DisplayIntegrator di; FakeBus bus; max7219::Display<FakeBus> lcd(bus);
  lcd.begin();
  struct K { int row; uint8_t k; } seq[] = {{6,8},{6,2},{5,2},{4,2},{0,1},{5,4+4}};   // clock,1,2,3,0, (AM below)
  (void)seq;
  auto run = [&](double s) { uint64_t n = (uint64_t)(s * 250000.0 / 6.0); uint8_t p[4];
    for (uint64_t i = 1; i <= n; i++) { cpu.step(); b.instr++; di.sample(b.r, b.o);
      if (i % 12000 == 0) { di.snapshot(p, 0.4f); if (p[0]|p[1]|p[2]|p[3]) lcd.show(p); } } };
  auto press = [&](int row, uint8_t k) { b.rows[row] |= k; run(0.3); b.rows[row] &= ~k; run(0.3); };
  run(0.5);
  press(6, 8);          // Time/Clock
  press(6, 2);          // 1
  press(5, 2);          // 2
  press(4, 2);          // 3
  press(0, 1);          // 0
  press(5, 8);          // Time Row AM 1  (AM)
  press(6, 8);          // Time/Clock again -> show time
  printf("MAX7219 digit registers after setting 12:30 (reg 0x01..0x04 = D1..D4, D1 = rightmost):\n");
  const char *name[4] = {"D1 (units of min)", "D2 (tens of min)", "D3 (units of hr)", "D4 (tens of hr)"};
  for (int d = 0; d < 4; d++) {
    int digit = -1; for (int i = 0; i < 10; i++) if (max_font[i] == bus.reg[1 + d]) digit = i;
    printf("  reg 0x%02X = 0x%02X  -> %c   %s\n", 1 + d, bus.reg[1 + d], digit < 0 ? '?' : '0' + digit, name[d]);
  }
  printf("scan limit reg = %d, SPI writes total = %d\n", bus.reg[0x0B], bus.writes);
}
