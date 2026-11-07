// ramdump: boot-model K8 (high T instrs after boot), run S seconds, hold Temp/Actual 0.6 s, dump RAM + display.
//   ./ramdump <T> <seconds>
#include <stdio.h>
#include <stdlib.h>
#include "tms0970.h"
#include "display_integrator.h"
#include "tithermos_tables.h"
struct Board : tms0970::Io {
  uint8_t rows[8] = {0,0,0,0x04,0,0,0,0x04}; uint8_t o = 0; uint16_t r = 0; uint64_t instr = 0; long T = 0;
  uint8_t read_k() override { uint8_t k = 0; for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) { if (((instr * 720) / 250000) & 1) k |= 1; }
    if ((o & 0x88) && (long)instr >= T) k |= 8; return k; }
  void write_o(uint8_t v) override { o = v; } void write_r(uint16_t v) override { r = v; } };
int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board b; b.T = atol(argv[1]); tms0970::Core cpu(t, b);
  uint64_t n = (uint64_t)(atof(argv[2]) * 250000.0 / 6.0);
  for (uint64_t i = 0; i < n; i++) { cpu.step(); b.instr++; }
  b.rows[6] |= 4; DisplayIntegrator d; uint8_t p[4] = {0,0,0,0}, last[4] = {0,0,0,0};
  for (uint64_t i = 1; i <= 25000; i++) { cpu.step(); b.instr++; d.sample(b.r, b.o); if (i % 4167 == 0) { d.snapshot(p); if (p[0]|p[1]|p[2]|p[3]) for (int k=0;k<4;k++) last[k]=p[k]; } }
  printf("T=%ld display raw %02X %02X %02X %02X\n", b.T, last[0], last[1], last[2], last[3]);
  for (int x = 0; x < 4; x++) { printf("X=%d:", x); for (int y = 0; y < 16; y++) printf(" %X", cpu.ram(x << 4 | y)); printf("\n"); }
}
