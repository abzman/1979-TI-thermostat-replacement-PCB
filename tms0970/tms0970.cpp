#include "tms0970.h"
#include <string.h>

namespace tms0970 {
namespace {

// fixed instructions (TMS0970 subset used by this part)
enum : uint16_t {
  F_BR = 1 << 0, F_CALL = 1 << 1, F_LDP = 1 << 2, F_TDO = 1 << 3, F_COMX = 1 << 4,
  F_LDX = 1 << 5, F_SBIT = 1 << 6, F_RBIT = 1 << 7, F_SETR = 1 << 8, F_RETN = 1 << 9
};

// microinstructions: bit order is the order of MAME's md[15] table in tms0970.cpp
enum : uint16_t {
  M_CKM = 1 << 0, M_CKP = 1 << 1, M_YTP = 1 << 2, M_MTP = 1 << 3, M_ATN = 1 << 4,
  M_NATN = 1 << 5, M_MTN = 1 << 6, M_15TN = 1 << 7, M_CKN = 1 << 8, M_NE = 1 << 9,
  M_C8 = 1 << 10, M_CIN = 1 << 11, M_AUTA = 1 << 12, M_AUTY = 1 << 13, M_STO = 1 << 14
};

constexpr uint8_t PC_MASK = 0x3f;   // 6-bit program counter
constexpr uint8_t X_BITS = 2;
constexpr uint8_t X_MASK = 3;

inline uint8_t rev4(uint8_t v) {
  return ((v & 1) << 3) | ((v & 2) << 1) | ((v & 4) >> 1) | ((v & 8) >> 3);
}
inline uint8_t rev5(uint8_t v) {
  return ((v & 1) << 4) | ((v & 2) << 2) | (v & 4) | ((v & 8) >> 2) | ((v & 16) >> 4);
}

}  // namespace

Core::Core(const Tables &t, Io &io) : t_(t), io_(io) {
  predecode();
  reset();
}

// Port of tms0970_cpu_device::device_reset() pre-decode step.
void Core::predecode() {
  static const uint16_t id[8] = {F_LDP, F_TDO, F_COMX, F_LDX, F_SBIT, F_RBIT, F_SETR, F_RETN};
  for (int op = 0; op < 256; op++) {
    uint16_t fixed = 0;
    if (op & 0x80) fixed = (op & 0x40) ? F_CALL : F_BR;   // upper half: always branch/call

    uint16_t imask = t_.ipla[op];
    uint8_t msel = imask & 0x1f;                          // 5 output bits select a microinstruction
    if ((imask & 0x40) && (imask & 0x20) == 0)
      msel = (op & 0xf) | ((op >> 1) & 0x10);             // R2,R4-R7 select it directly
    msel = rev5(msel);                                    // lines are reversed

    uint16_t mmask = (t_.mpla[msel] ^ 0x09fe) & 0x7fff;   // invert active-negative
    micro_decode_[op] = mmask;

    for (int bit = 0; bit < 8; bit++)
      if (imask & (0x80 << bit)) fixed |= id[bit];        // other ipla terms: fixed instructions
    fixed_decode_[op] = fixed;
  }
}

void Core::reset() {
  memset(ram_, 0, sizeof(ram_));
  pc_ = sr_ = a_ = x_ = y_ = clatch_ = status_ = 0;
  c4_ = cki_ = p_ = n_ = carry_in_ = adder_out_ = ram_in_ = 0;
  pa_ = pb_ = 0xf;                       // reset vector: page 15, pc 0
  opcode_ = 0; fixed_ = 0; micro_ = 0;
  rom_address_ = 0; cur_addr_ = 0;
  ram_out_ = -1; ram_address_ = 0;
  r_ = 0; o_ = 0;
  io_.write_r(0);
  write_o_reg(0);
  io_.write_r(0);
}

void Core::write_o_reg(uint8_t index) {
  o_ = t_.spla[index & 7];
  io_.write_o(o_);
}

uint8_t Core::cki_bus() {
  switch (opcode_ & 0xf8) {
    case 0x08:                                   // 00001XXX: K inputs
      return io_.read_k() & 0xf;
    case 0x30: case 0x38:                        // 0011XXXX: select bit
      return (1 << (c4_ >> 2)) ^ 0xf;
    case 0x00:                                   // R2,3,4 NANDed then ORed with R1: 00000XXX valid too
    case 0x40: case 0x48: case 0x50: case 0x58:
    case 0x60: case 0x68: case 0x70: case 0x78:  // 01XXXXXX: constant
      return c4_;
    default:
      return 0;
  }
}

void Core::step() {
  // ---- subcycle 0: branch/call/return part 2, K valid, read RAM, clear ALU inputs
  if (fixed_ & F_BR) {                           // op_br1
    if (status_) {
      if (clatch_ == 0) pa_ = pb_;
      pc_ = opcode_ & PC_MASK;
    }
  }
  if (fixed_ & F_CALL) {                         // op_call1
    if (status_) {
      uint8_t prev_pa = pa_;
      if (!clatch_) { clatch_ = 1; sr_ = pc_; pa_ = pb_; }
      pb_ = prev_pa;
      pc_ = opcode_ & PC_MASK;
    }
  }
  if (fixed_ & F_RETN) {                         // op_retn1
    if (clatch_) { clatch_ = 0; pc_ = sr_; }
    pa_ = pb_;
  }
  cki_ = cki_bus();
  ram_in_ = ram_[ram_address_] & 0xf;
  p_ = 0; n_ = 0; carry_in_ = 0;

  // ---- subcycle 1: ROM address, ALU inputs
  rom_address_ = (uint16_t(pa_) << 6) | pc_;     // chapter address is always 0 on this part
  if (micro_ & M_15TN) n_ |= 0xf;
  if (micro_ & M_ATN)  n_ |= a_;
  if (micro_ & M_NATN) n_ |= (~a_ & 0xf);
  if (micro_ & M_CKN)  n_ |= cki_;
  if (micro_ & M_MTN)  n_ |= ram_in_;
  if (micro_ & M_CKP)  p_ |= cki_;
  if (micro_ & M_MTP)  p_ |= ram_in_;
  if (micro_ & M_YTP)  p_ |= y_;
  if (micro_ & M_CIN)  carry_in_ |= 1;

  // ---- subcycle 2: ALU, fixed opcodes, RAM write
  adder_out_ = p_ + n_ + carry_in_;
  int carry_out = (adder_out_ >> 4) & 1;
  int status = 1;
  ram_out_ = -1;
  if (micro_ & M_C8) status &= carry_out;
  if (micro_ & M_NE) status &= (n_ != p_);
  if (micro_ & M_CKM) ram_out_ = cki_;
  if (micro_ & M_STO) ram_out_ = a_;

  if (fixed_ & F_SBIT) {                         // set memory bit
    if (ram_out_ == -1) ram_out_ = ram_in_;
    ram_out_ |= (cki_ ^ 0xf);
  }
  if (fixed_ & F_RBIT) {                         // reset memory bit
    if (ram_out_ == -1) ram_out_ = ram_in_;
    ram_out_ &= cki_;
  }
  if (fixed_ & F_SETR) {                         // tms0970 SETR: DDIG coincidence of opla row(s) and segment pla row(s)
    int ddig = (~t_.opla[a_ & 0xf] & o_ & 0xff) ? 0 : 1;
    r_ = (r_ & ~(1u << y_)) | (uint32_t(ddig) << y_);
  }
  if (fixed_ & F_TDO) {                          // tms0970 TDO: segment pla -> O, latch R
    write_o_reg(a_ & 7);
    io_.write_r(r_ & 0x7ff);
  }
  if (fixed_ & F_LDX)  x_ = c4_ >> (4 - X_BITS);
  if (fixed_ & F_COMX) x_ ^= X_MASK;
  if (fixed_ & F_LDP)  pb_ = c4_;

  status_ = status;
  if (ram_out_ != -1) ram_[ram_address_] = ram_out_ & 0xf;

  // ---- subcycle 4: register store, fetch next opcode
  if (micro_ & M_AUTA) a_ = adder_out_ & 0xf;
  if (micro_ & M_AUTY) y_ = adder_out_ & 0xf;

  cur_addr_ = rom_address_;
  opcode_ = t_.rom[rom_address_ & 0x3ff];
  c4_ = rev4(opcode_ & 0xf);
  fixed_ = fixed_decode_[opcode_];
  micro_ = micro_decode_[opcode_];

  // next_pc(): the program counter is an LFSR
  {
    int high = 1 << 5;
    int fb = ((pc_ << 1) & high) == (pc_ & high);
    if (pc_ == (PC_MASK >> 1)) fb = 1;
    else if (pc_ == PC_MASK) fb = 0;
    pc_ = ((pc_ << 1) | fb) & PC_MASK;
  }
  ram_address_ = (x_ << 4) | y_;
}

}  // namespace tms0970
