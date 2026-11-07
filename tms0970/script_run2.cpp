// script_run: drive the ROM with a key script and print the display after each step.
//   ./script_run "time-clock:0.3 1:0.3 2:0.3 ..." [settle_s] [sysrow] [moderow]
// Each token is key:hold_seconds ; 'wait:s' just waits; gap of 0.3 s is inserted after every key.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
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
  long K8T = -1; int K8POL = 0, K8EDGE = 0; uint64_t last_edge = 0;   // K8 model: ((instr-last_edge)>=T)^POL
  uint8_t read_k() override {
    uint8_t k = 0;
    for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if (o & 0x88) { bool k8 = (K8T >= 0) ? ((long)(instr - last_edge) >= K8T) : false; if (k8 ^ (K8POL != 0)) k |= 8; }
    return k;
  }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override {
    if ((v ^ r) & 0x80) { bool rise = v & 0x80; if (K8EDGE == 2 || (K8EDGE == 1) == rise) last_edge = instr; }
    r = v; }
};

Board b; tms0970::Core *cpu; DisplayIntegrator disp;
// run for `secs`, return the display from the last 100 ms window that had >=1 lit segment ("sticky"), plus whether blank at end
static void run(double secs, uint8_t last[4], bool &blank_end) {
  uint64_t n = (uint64_t)(secs * IPS), win = 4167; uint8_t p[4];
  blank_end = true;
  for (uint64_t i = 1; i <= n; i++) {
    cpu->step(); b.instr++; disp.sample(b.r, b.o);
    if (i % win == 0) {
      disp.snapshot(p);
      bool any = p[0] | p[1] | p[2] | p[3];
      blank_end = !any;
      if (any) memcpy(last, p, 4);
    }
  }
}
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  cpu = new tms0970::Core(t, b);
  if (getenv("K8T")) b.K8T = atol(getenv("K8T"));
  if (getenv("K8POL")) b.K8POL = atoi(getenv("K8POL"));
  if (getenv("K8EDGE")) b.K8EDGE = atoi(getenv("K8EDGE"));
  if (argc > 3) b.rows[3] = (uint8_t)strtol(argv[3], 0, 0);
  if (argc > 4) b.rows[7] = (uint8_t)strtol(argv[4], 0, 0);
  uint8_t last[4] = {0,0,0,0}; bool blank;
  run(0.5, last, blank);                         // boot
  std::string s = argv[1]; size_t pos = 0; std::vector<std::string> toks;
  while (pos < s.size()) { size_t e = s.find(' ', pos); if (e == std::string::npos) e = s.size(); if (e > pos) toks.push_back(s.substr(pos, e - pos)); pos = e + 1; }
  for (auto &tk : toks) {
    size_t c = tk.find(':'); std::string name = tk.substr(0, c); double hold = c == std::string::npos ? 0.3 : atof(tk.c_str() + c + 1);
    const KeyDef *kd = nullptr; for (auto &k : kKeys) if (name == k.name) kd = &k;
    memset(last, 0, 4);
    if (kd) b.rows[kd->row] |= kd->k;
    run(hold, last, blank);
    if (kd) b.rows[kd->row] &= ~kd->k;
    uint8_t after[4]; memcpy(after, last, 4);
    printf("%-12s held %.2fs -> [%c%c%c%c]%s", name.c_str(), hold, seg(after[0]), seg(after[1]), seg(after[2]), seg(after[3]), blank ? " (blank at end)" : "");
    memset(last, 0, 4);
    run(0.3, last, blank);                       // gap
    printf("   | after release [%c%c%c%c]%s\n", seg(last[0]), seg(last[1]), seg(last[2]), seg(last[3]), blank ? " (blank)" : "");
  }
  if (argc > 2) { memset(last,0,4); run(atof(argv[2]), last, blank); printf("settle %.1fs -> [%c%c%c%c]%s\n", atof(argv[2]), seg(last[0]),seg(last[1]),seg(last[2]),seg(last[3]), blank?" (blank)":""); }
}
