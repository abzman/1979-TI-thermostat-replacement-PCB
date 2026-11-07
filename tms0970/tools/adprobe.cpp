// adprobe: log how the ROM uses D8 (R7) and K reads on SB/SD/SP, with K8 model selectable.
//   ./adprobe <key|none> <seconds> <T_instr> <pol>   K8 = (instr_since_R7_fall >= T) ^ pol ; T<0 => K8 stuck 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tms0970.h"
#include "tithermos_tables.h"
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04};
  uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0, r7fall = 0, r7rise = 0;
  long T = -1; int pol = 0; bool verbose = true;
  uint64_t kreads_ad = 0, kreads_ad_k8hi = 0, r7edges = 0;
  uint8_t read_k() override {
    uint8_t k = 0;
    for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) {
      if (((instr * 720) / 250000) & 1) k |= 1;
      bool run = !(r & 0x80);
      bool k8 = run && T >= 0 && (long)(instr - r7fall) >= T;
      k8 ^= (pol != 0);
      if (k8 && (o & 0x88)) k |= 8;     // A/D output defined on SD and SP rows
      if (run) { kreads_ad++; if (k8) kreads_ad_k8hi++; }
    }
    return k;
  }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override {
    if ((v ^ r) & 0x80) {
      r7edges++;
      if (v & 0x80) r7rise = instr; else r7fall = instr;
      if (verbose && r7edges <= 24)
        printf("  i=%8llu  R7 %s  (R=%03X)  %s%llu instrs\n", (unsigned long long)instr, (v & 0x80) ? "rise" : "fall", v,
               (v & 0x80) ? "high-for " : "", (unsigned long long)((v & 0x80) ? 0 : instr - r7rise));
    }
    r = v;
  }
};
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b);
  if (strcmp(argv[1], "none")) { if (!strcmp(argv[1], "temp-actual")) b.rows[6] |= 4; else if (!strcmp(argv[1],"time-clock")) b.rows[6] |= 8; }
  double secs = atof(argv[2]); b.T = atol(argv[3]); b.pol = atoi(argv[4]);
  uint64_t n = (uint64_t)(secs * 250000.0 / 6.0);
  for (uint64_t i = 0; i < n; i++) { cpu.step(); b.instr++; }
  printf("R7 edges: %llu   K reads while A/D running: %llu (K8 high on %llu)\n",
         (unsigned long long)b.r7edges, (unsigned long long)b.kreads_ad, (unsigned long long)b.kreads_ad_k8hi);
}
