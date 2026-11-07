// poketest: K8 never asserted; force RAM[3][F]:[3][E] (tens:units) to a temperature every N instructions,
// run S seconds, press Temp/Actual for 0.6 s and print what the display shows.
//   ./poketest <temp> <seconds> <poke_every_n_instr> [K8 mode: 0 none, 1 high on SD/SP]
#include <stdio.h>
#include <stdlib.h>
#include "tms0970.h"
#include "display_integrator.h"
#include "tithermos_tables.h"
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; int k8 = 0;
  uint8_t read_k() override { uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if (k8 && (o & 0x88)) k |= 8; return k; }
  void write_o(uint8_t v) override { o = v; } void write_r(uint16_t v) override { r = v; } };
static const uint8_t kSeg[10] = {0x7E,0x30,0x6D,0x79,0x33,0x5B,0x5F,0x70,0x7F,0x7B};
static char dec(uint8_t p) { if (!p) return ' '; for (int i = 0; i < 10; i++) if (kSeg[i] == p) return '0' + i; return '?'; }
int main(int argc, char **argv) {
  int temp = atoi(argv[1]); double secs = atof(argv[2]); long every = atol(argv[3]);
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; if (argc > 4) b.k8 = atoi(argv[4]); tms0970::Core cpu(t, b);
  auto poke = [&]() { cpu.set_ram(0x3E, temp % 10); cpu.set_ram(0x3F, temp / 10); };
  uint64_t n = (uint64_t)(secs * 250000.0 / 6.0);
  for (uint64_t i = 0; i < n; i++) { cpu.step(); b.instr++; if (every && i % every == 0) poke(); }
  b.rows[6] |= 4; DisplayIntegrator d; uint8_t p[4], last[4] = {0,0,0,0};
  for (uint64_t i = 1; i <= 25000; i++) { cpu.step(); b.instr++; if (every && i % every == 0) poke(); d.sample(b.r, b.o);
    if (i % 4167 == 0) { d.snapshot(p); if (p[0]|p[1]|p[2]|p[3]) for (int k=0;k<4;k++) last[k]=p[k]; } }
  printf("poke=%d every=%ld K8=%d -> display (digit0 rightmost): [%c%c%c%c]  RAM[3][F:E]=%X%X  R5(heat/cool call)=%d\n", temp, every, b.k8,
    dec(last[3]), dec(last[2]), dec(last[1]), dec(last[0]), cpu.ram(0x3F), cpu.ram(0x3E), (b.r >> 5) & 1);
}
