// dyntrace: dump executed addresses around the Nth execution of a given ROM address.
//   ./dyntrace <key|none> <seconds> <addr_hex> <nth> <before> <after>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "tms0970.h"
#include "tithermos_tables.h"
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; int k8 = 0;
  uint8_t read_k() override {
    uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if (k8 && (o & 0x88)) k |= 8;
    return k; }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override { r = v; }
};
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b);
  if (!strcmp(argv[1], "temp-actual")) b.rows[6] |= 4;
  if (getenv("K8")) b.k8 = atoi(getenv("K8"));
  int target = strtol(argv[3], 0, 16), nth = atoi(argv[4]), before = atoi(argv[5]), after = atoi(argv[6]);
  uint64_t n = (uint64_t)(atof(argv[2]) * 250000.0 / 6.0);
  std::vector<int> hist; int hits = 0; long stop_after = -1;
  for (uint64_t i = 0; i < n; i++) {
    int exec_addr = cpu.fetched_address();     // address of instruction about to execute in this step
    hist.push_back(exec_addr);
    cpu.step(); b.instr++;
    if (stop_after < 0 && exec_addr == target && ++hits == nth) stop_after = after;
    else if (stop_after > 0) stop_after--;
    if (stop_after == 0) break;
  }
  if (stop_after != 0) { printf("target not reached (hits=%d)\n", hits); return 1; }
  size_t end = hist.size(), start = end > (size_t)(before + after + 1) ? end - (before + after + 1) : 0;
  for (size_t i = start; i < end; i++) printf("%03X%s\n", hist[i], (int)i == (int)(end - 1 - after) ? " <==" : "");
}
