// explore: run the ROM for N emulated seconds, print decoded display at intervals.
//   ./explore <key|none> <seconds> <interval_s> [k8_mode]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tms0970.h"
#include "display_integrator.h"
#include "tithermos_tables.h"

struct KeyDef { const char *name; int row; uint8_t k; };
static const KeyDef kKeys[] = {
  {"0",0,1},{"5",0,2},{"temp-pm2",0,4},{"time-pm2",0,8},
  {"9",2,1},{"4",2,2},{"temp-pm1",2,4},{"time-pm1",2,8},
  {"8",4,1},{"3",4,2},{"temp-am2",4,4},{"time-am2",4,8},
  {"7",5,1},{"2",5,2},{"temp-am1",5,4},{"time-am1",5,8},
  {"6",6,1},{"1",6,2},{"temp-actual",6,4},{"time-clock",6,8},
};
static const double IPS = 250000.0 / 6.0;

static char seg(uint8_t s) {
  switch (s) {
    case 0x00: return ' '; case 0x3F: return '0'; case 0x06: return '1'; case 0x5B: return '2';
    case 0x4F: return '3'; case 0x66: return '4'; case 0x6D: return '5'; case 0x7D: return '6';
    case 0x07: return '7'; case 0x7F: return '8'; case 0x6F: return '9'; case 0x40: return '-';
    default: return '?';
  }
}

struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04};
  uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0;
  uint8_t read_k() override {
    uint8_t k = 0;
    for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if ((instr / 347) & 1) k |= 1; }
    return k;
  }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override { r = v; }
};

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage\n"); return 1; }
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b);
  if (strcmp(argv[1], "none")) {
    bool f = false;
    for (auto &k : kKeys) if (!strcmp(argv[1], k.name)) { b.rows[k.row] |= k.k; f = true; }
    if (!f) { fprintf(stderr, "bad key\n"); return 1; }
  }
  double secs = atof(argv[2]), iv = atof(argv[3]);
  DisplayIntegrator d; uint8_t p[4];
  uint64_t total = (uint64_t)(secs * IPS), every = (uint64_t)(iv * IPS), win = 2000;
  for (uint64_t i = 1; i <= total; i++) {
    cpu.step(); b.instr++;
    d.sample(b.r, b.o);
    if (i % win == 0 && i % every) d.snapshot(p);
    if (i % every == 0) {
      d.snapshot(p);
      printf("t=%7.2fs  [%c%c%c%c]  raw %02X %02X %02X %02X  R=%03X\n", i / IPS,
             seg(p[0]), seg(p[1]), seg(p[2]), seg(p[3]), p[0], p[1], p[2], p[3], b.r);
    }
  }
}
