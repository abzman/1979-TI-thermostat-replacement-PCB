// Host-side smoke test / trace tool.
//   g++ -std=c++17 -O2 smoke_main.cpp tms0970.cpp -o smoke
//   ./smoke                      run ~3 s of emulated time, print the display
//   ./smoke time-clock           hold a key for the run
//   ./smoke trace 200            print the first 200 fetch addresses/opcodes (compare with MAME's 'trace')
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tms0970.h"
#include "display_integrator.h"
#include "tithermos_tables.h"

// Key matrix from MAME's tithermos INPUT_PORTS: rows are driven by O (SA,SB,SC,SD,SE,SF,SG,SP).
struct KeyDef { const char *name; int row; uint8_t k; };
static const KeyDef kKeys[] = {
  {"0",0,1},{"5",0,2},{"temp-pm2",0,4},{"time-pm2",0,8},
  {"9",2,1},{"4",2,2},{"temp-pm1",2,4},{"time-pm1",2,8},
  {"8",4,1},{"3",4,2},{"temp-am2",4,4},{"time-am2",4,8},
  {"7",5,1},{"2",5,2},{"temp-am1",5,4},{"time-am1",5,8},
  {"6",6,1},{"1",6,2},{"temp-actual",6,4},{"time-clock",6,8},
};

class Board : public tms0970::Io {
 public:
  uint8_t rows[8] = {0, 0, 0, 0x04 /*system=heat*/, 0, 0, 0, 0x04 /*mode=constant*/};
  uint8_t o = 0;
  uint16_t r = 0;
  uint64_t instr = 0;
  uint8_t read_k() override {
    uint8_t k = 0;
    for (int i = 0; i < 8; i++) if (o & (1 << i)) k |= rows[i];
    if (o & 0x8a) {                                  // SB/SD/SP: 60 Hz on K1 (41.67k instr/s -> toggle every ~347)
      if ((instr / 347) & 1) k |= 1;
      // K8 (A/D comparator) not modelled yet
    }
    return k;
  }
  void write_o(uint8_t v) override { o = v; }
  void write_r(uint16_t v) override { r = v; }
};

static void print_display(const uint8_t d[4]) {
  // assumes SA..SG = segments a..g (standard layout) - verify against the real display
  for (int line = 0; line < 5; line++) {
    for (int i = 0; i < 4; i++) {
      uint8_t s = d[i];
      auto on = [&](int b) { return (s >> b) & 1; };
      switch (line) {
        case 0: printf(" %c  ", on(0) ? '_' : ' '); break;
        case 1: printf("%c %c ", on(5) ? '|' : ' ', on(1) ? '|' : ' '); break;
        case 2: printf(" %c  ", on(6) ? '_' : ' '); break;
        case 3: printf("%c %c ", on(4) ? '|' : ' ', on(2) ? '|' : ' '); break;
        case 4: printf(" %c  ", on(3) ? '_' : ' '); break;
      }
    }
    printf("\n");
  }
}

int main(int argc, char **argv) {
  tms0970::Tables t{kRom, kIpla, kMpla, kOpla, kSpla};
  Board board;
  tms0970::Core cpu(t, board);

  if (argc >= 3 && !strcmp(argv[1], "trace")) {
    int n = atoi(argv[2]);
    for (int i = 0; i < n; i++) {
      cpu.step(); board.instr++;
      printf("%03X: %02X\n", cpu.fetched_address(), cpu.opcode());
    }
    return 0;
  }

  if (argc >= 2) {
    bool found = false;
    for (auto &k : kKeys) if (!strcmp(argv[1], k.name)) { board.rows[k.row] |= k.k; found = true; }
    if (!found) { fprintf(stderr, "unknown key %s\n", argv[1]); return 1; }
  }

  DisplayIntegrator disp;
  uint8_t pat[4];
  const uint64_t total = 125000;       // ~3 s at 41.67k instructions/s
  for (uint64_t i = 0; i < total; i++) {
    cpu.step(); board.instr++;
    disp.sample(board.r, board.o);
    if ((i + 1) % 2000 == 0 && i + 1 < total) disp.snapshot(pat);   // ~48 ms windows, discarded
  }
  disp.snapshot(pat);
  printf("digit patterns: %02X %02X %02X %02X   R=%03X O=%02X\n", pat[0], pat[1], pat[2], pat[3], board.r, board.o);
  print_display(pat);
  return 0;
}
