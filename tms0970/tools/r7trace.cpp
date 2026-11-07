// r7trace: dump executed instructions (addr, A, X, Y, status, RAM[x,y]) around the Nth R7 (D8) edge.
//   ./r7trace <edge_n> <before> <after> [K8 const 0/1]
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "tms0970.h"
#include "tithermos_tables.h"
struct Rec { int addr, a, x, y, st, ram; uint16_t r; uint8_t o, k; };
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; int k8 = 0;
  uint8_t lastk = 0; int edges = 0; int edge_idx = -1; int want = 1;
  uint8_t read_k() override { uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if (k8 && (o & 0x88)) k |= 8; lastk = k; return k; }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override { if ((v ^ r) & 0x80) { if (++edges == want) edge_idx = (int)instr; } r = v; }
};
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; tms0970::Core cpu(t, b);
  b.want = atoi(argv[1]); int before = atoi(argv[2]), after = atoi(argv[3]); if (argc > 4) b.k8 = atoi(argv[4]);
  std::vector<Rec> h; h.reserve(2000000);
  for (uint64_t i = 0; i < 2000000; i++) {
    int ea = cpu.fetched_address();
    cpu.step(); b.instr++;
    h.push_back({ea, cpu.a(), cpu.x(), cpu.y(), cpu.status(), cpu.ram((cpu.x() << 4) | cpu.y()), b.r, b.o, b.lastk});
    if (b.edge_idx >= 0 && (int)b.instr > b.edge_idx + after) break;
  }
  if (b.edge_idx < 0) { printf("edge %d not reached\n", b.want); return 1; }
  for (int i = b.edge_idx - before; i <= b.edge_idx + after; i++) {
    Rec &r = h[i];
    printf("%03X a=%X x=%d y=%X st=%d R=%03X O=%02X K=%X%s\n", r.addr, r.a, r.x, r.y, r.st, r.r, r.o, r.k, i == b.edge_idx ? "  <== R7 changes during this instr" : "");
  }
}
