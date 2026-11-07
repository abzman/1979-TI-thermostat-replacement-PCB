// ramwatch: log every instruction that changes watched RAM nibbles (boot-model K8), and print code leading to the last write.
//   ./ramwatch <T> <seconds> <hex addr list e.g. 3E,3F,16,17,18>  <before>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include "tms0970.h"
#include "tithermos_tables.h"
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; long T = 0;
  uint8_t read_k() override { uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if ((o & 0x88) && (long)instr >= T) k |= 8; return k; }
  void write_o(uint8_t v) override { o = v; } void write_r(uint16_t v) override { r = v; } };
struct Rec { int addr, a, x, y, st; };
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; b.T = atol(argv[1]); tms0970::Core cpu(t, b);
  uint64_t n = (uint64_t)(atof(argv[2]) * 250000.0 / 6.0); int before = atoi(argv[4]);
  std::vector<int> watch; { char *s = strdup(argv[3]); for (char *p = strtok(s, ","); p; p = strtok(0, ",")) watch.push_back(strtol(p, 0, 16)); }
  std::vector<Rec> h; h.reserve(n + 1);
  uint8_t prev[64]; for (int i = 0; i < 64; i++) prev[i] = cpu.ram(i);
  std::vector<std::pair<long,int>> events;   // (history index, ram address)
  for (uint64_t i = 0; i < n; i++) {
    int ea = cpu.fetched_address();
    cpu.step(); b.instr++;
    h.push_back({ea, cpu.a(), cpu.x(), cpu.y(), cpu.status()});
    for (int w : watch) if (cpu.ram(w) != prev[w]) events.push_back({(long)h.size() - 1, w});
    for (int k = 0; k < 64; k++) prev[k] = cpu.ram(k);
  }
  printf("write events to watched nibbles: %zu\n", events.size());
  // summarize writers (pc) per address
  for (int w : watch) { int cnt = 0; std::vector<int> pcs; for (auto &e : events) if (e.second == w) { cnt++; int pc = h[e.first].addr; bool seen = false; for (int q : pcs) if (q == pc) seen = true; if (!seen) pcs.push_back(pc); }
    printf("  RAM[%X][%X]: %d writes, writer PCs:", w >> 4, w & 15, cnt); for (int q : pcs) printf(" %03X", q); printf("\n"); }
  // window before the last event
  if (!events.empty()) { long idx = events.back().first;
    printf("--- last write (RAM[%X][%X]) at history %ld; %d instructions before it ---\n", events.back().second >> 4, events.back().second & 15, idx, before);
    for (long i = idx - before; i <= idx; i++) printf("%03X a=%X x=%d y=%X st=%d%s\n", h[i].addr, h[i].a, h[i].x, h[i].y, h[i].st, i == idx ? "  <== write" : ""); }
}
