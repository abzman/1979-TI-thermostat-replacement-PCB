// Integrates the TMS0970's time-multiplexed display outputs into steady digit patterns.
//
// The ROM scans segment by segment: O carries one segment strobe (SA..SG) at a time and
// R0..R3 (D1..D4) say which digits have that segment lit.  So at any instant the lit
// cells are the outer product (R digits) x (O segments), exactly what MAME's PWM_DISPLAY
// matrix(R, O) models.  Call sample() once per emulated instruction, then snapshot()
// every 20-50 ms of emulated time to get a stable pattern for the MAX7219.
#pragma once
#include <stdint.h>
#include <string.h>

class DisplayIntegrator {
 public:
  static constexpr int kDigits = 4;
  static constexpr int kSegs = 7;

  void sample(uint16_t r, uint8_t o) {
    const uint8_t digits = r & 0x0f;
    const uint8_t segs = o & 0x7f;       // O7 (SP) is a key strobe, not a segment
    if (!digits || !segs) { n_++; return; }
    for (int d = 0; d < kDigits; d++)
      if (digits & (1 << d))
        for (int s = 0; s < kSegs; s++)
          if (segs & (1 << s)) on_[d][s]++;
    n_++;
  }

  // Returns bit pattern per digit (bit0 = segment a ... bit6 = g) and restarts the window.
  // A segment counts as lit if it was on for more than `min_fraction` of the brightest cell.
  void snapshot(uint8_t out[kDigits], float min_fraction = 0.25f) {
    uint32_t peak = 0;
    for (auto &row : on_) for (uint32_t c : row) if (c > peak) peak = c;
    for (int d = 0; d < kDigits; d++) {
      out[d] = 0;
      for (int s = 0; s < kSegs; s++)
        if (peak && on_[d][s] >= peak * min_fraction) out[d] |= 1 << s;
    }
    memset(on_, 0, sizeof(on_));
    n_ = 0;
  }

 private:
  uint32_t on_[kDigits][kSegs] = {};
  uint32_t n_ = 0;
};
