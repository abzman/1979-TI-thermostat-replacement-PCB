// MAX7219 back end for the TI thermostat board.
//
// Wiring (from the board schematic):
//   MAX7219 SEG_A..SEG_G -> display A..G, DP -> DP
//   MAX7219 DIG_0..DIG_7 -> display D1..D8
//   LOAD = IO5, CLK = IO18, DIN = IO23 (VSPI)
// ROM side (MAME driver / patent US4298946):
//   O0..O6 (SA..SG) = segments a..g,  R0..R3 (D1..D4) = digits, R0 = RIGHTMOST digit.
//   R5 (D6) is the triac gate and R7 (D8) the A/D reset -- NOT display digits, so
//   MAX7219 digits 4..7 are never written (and scan limit is set to 4 digits).
#pragma once
#include <stdint.h>

namespace max7219 {

// ROM segment byte (bit0=a .. bit6=g)  ->  MAX7219 data byte (bit6=A .. bit0=G, bit7=DP)
inline uint8_t from_rom_segments(uint8_t s) {
  uint8_t o = 0;
  for (int i = 0; i < 7; i++)
    if (s & (1 << i)) o |= 1 << (6 - i);
  return o;
}

enum Reg : uint8_t {
  kDigit0 = 0x01,        // digit n is register 0x01 + n
  kDecodeMode = 0x09, kIntensity = 0x0A, kScanLimit = 0x0B, kShutdown = 0x0C, kTest = 0x0F
};

// `Bus` just needs:  void write(uint8_t reg, uint8_t value);
template <class Bus>
class Display {
 public:
  static constexpr int kDigits = 4;
  explicit Display(Bus &bus) : bus_(bus) {}

  void begin(uint8_t intensity = 3) {
    bus_.write(kTest, 0);
    bus_.write(kDecodeMode, 0x00);          // raw segments, we do our own font (the ROM does)
    bus_.write(kScanLimit, kDigits - 1);    // scan only D1..D4: brighter, no ghosting on unused digits
    bus_.write(kIntensity, intensity & 0x0f);
    for (int d = 0; d < 8; d++) bus_.write(kDigit0 + d, 0);
    bus_.write(kShutdown, 1);               // normal operation
    for (int d = 0; d < kDigits; d++) shadow_[d] = 0;
  }

  // pattern[d]: ROM-order segment byte for digit d (d=0 is R0, the rightmost digit).
  // Only writes digits that changed.
  void show(const uint8_t pattern[kDigits]) {
    for (int d = 0; d < kDigits; d++) {
      uint8_t v = from_rom_segments(pattern[d] & 0x7f);
      if (v != shadow_[d]) { shadow_[d] = v; bus_.write(kDigit0 + d, v); }
    }
  }
  void set_intensity(uint8_t i) { bus_.write(kIntensity, i & 0x0f); }
  void blank() { uint8_t z[kDigits] = {0, 0, 0, 0}; show(z); }

 private:
  Bus &bus_;
  uint8_t shadow_[kDigits] = {0, 0, 0, 0};
};

#ifdef ARDUINO
#include <SPI.h>
// 16-bit frames: [register][data], LOAD (chip select) = IO5. VSPI default pins: SCK 18, MOSI 23.
struct SpiBus {
  uint8_t cs = 5;
  void begin() { pinMode(cs, OUTPUT); digitalWrite(cs, HIGH); SPI.begin(18, -1, 23, cs); }
  void write(uint8_t reg, uint8_t val) {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs, LOW);
    SPI.transfer(reg); SPI.transfer(val);
    digitalWrite(cs, HIGH);
    SPI.endTransaction();
  }
};
#endif

}  // namespace max7219
