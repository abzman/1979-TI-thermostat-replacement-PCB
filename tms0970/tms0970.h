// Portable instruction-level TMS0970 core (TI Electronic Digital Thermostat).
//
// A dependency-free port of MAME's tms1k_base + tms0970 (BSD-3-Clause,
// copyright-holders: Wilbert Pol, hap).  MAME runs each instruction as 6 subcycles;
// here one call to step() runs all of them in order, which is equivalent as long as
// external inputs only need per-instruction resolution (they do: K is sampled once
// per instruction, outputs change once per instruction).
#pragma once
#include <stdint.h>

namespace tms0970 {

struct Tables {
  const uint8_t  *rom;   // 1024 bytes, MAME file order (address = page<<6 | pc, do not reorder)
  const uint16_t *ipla;  // 256 entries (15 bits)
  const uint16_t *mpla;  // 32 entries (15 bits)
  const uint8_t  *opla;  // 16 entries (8 bits)
  const uint8_t  *spla;  // 8 entries (8 bits)
};

// Pin interface.  Implement this to connect the CPU to the rest of the board.
class Io {
 public:
  virtual ~Io() {}
  virtual uint8_t read_k() = 0;          // K8..K1 as bits 3..0, sampled when the ROM reads K
  virtual void write_o(uint8_t o) = 0;   // O0..O7  (SA..SG, SP)
  virtual void write_r(uint16_t r) = 0;  // R0..R10 (D1.. ; D6 = R5 heat/cool call, D8 = R7 A/D reset)
};

class Core {
 public:
  Core(const Tables &t, Io &io);
  void reset();
  void step();  // execute exactly one instruction

  // current output latches (what the pins currently show)
  uint8_t  o() const { return o_; }
  uint16_t r() const { return r_ & 0x7ff; }

  // debug / differential-test accessors
  uint16_t fetched_address() const { return cur_addr_; }  // ROM address of the instruction that step() will run next
  uint8_t  opcode() const { return opcode_; }
  uint8_t  a() const { return a_; }
  uint8_t  x() const { return x_; }
  uint8_t  y() const { return y_; }
  uint8_t  status() const { return status_; }
  uint8_t  ram(uint8_t addr) const { return ram_[addr & 0x3f] & 0xf; }
  void     set_ram(uint8_t addr, uint8_t v) { ram_[addr & 0x3f] = v & 0xf; }

 private:
  void predecode();
  void write_o_reg(uint8_t index);
  uint8_t cki_bus();

  Tables t_;
  Io &io_;

  uint16_t fixed_decode_[256];
  uint16_t micro_decode_[256];
  uint8_t  ram_[64];

  // registers (names follow MAME)
  uint8_t  pc_, sr_, pa_, pb_, a_, x_, y_, clatch_, status_;
  uint8_t  c4_, cki_, p_, n_, carry_in_, adder_out_, ram_in_;
  uint8_t  opcode_;
  uint16_t fixed_, micro_, rom_address_, cur_addr_;
  uint32_t r_;
  uint8_t  o_;
  int      ram_out_;
  uint8_t  ram_address_;
};

}  // namespace tms0970
