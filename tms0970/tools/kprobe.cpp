// kprobe: histogram of (ROM address of the K-reading instruction, O strobe, R) and the K value returned.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <tuple>
#include "tms0970.h"
#include "tithermos_tables.h"
tms0970::Core *g;
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04};
  uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; int k8mode = 0; // 0 none, 1 always high on SD/SP, 2 always low (nothing)
  std::map<std::tuple<int,int,int,int>, long> h;
  uint8_t read_k() override {
    uint8_t k = 0;
    for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if (k8mode == 1 && (o & 0x88)) k |= 8;
    h[{g->fetched_address(), o, r & 0x7ff, k}]++;
    return k;
  }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override { r = v; }
};
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b); g = &cpu;
  b.k8mode = atoi(argv[2]);
  if (!strcmp(argv[1], "temp-actual")) b.rows[6] |= 4;
  uint64_t n = (uint64_t)(atof(argv[3]) * 250000.0 / 6.0);
  for (uint64_t i = 0; i < n; i++) { cpu.step(); b.instr++; }
  printf("%-6s %-4s %-4s %-3s  count\n", "addr", "O", "R", "K");
  for (auto &e : b.h) printf("%03X    %02X   %03X  %X   %ld\n", std::get<0>(e.first), std::get<1>(e.first), std::get<2>(e.first), std::get<3>(e.first), e.second);
}
