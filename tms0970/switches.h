// System / Mode slide switches.  Both hang off 74HC138 output 7 ("switchscan"):
//   address 7 selected -> output 7 goes LOW; a switch position that "connects" to a data pin pulls
//   that pin LOW through the switch, otherwise the external pull-up reads HIGH.
//     Mode  : Constant = neither IO36 nor IO39 low | Day/Night = IO36 low | Night = IO39 low
//     System: Heat     = neither IO34 nor IO35 low | Off       = IO34 low | Cool  = IO35 low
// The ROM sees them on strobe rows SD (System) and SP (Mode), K bits 0x06:
//   System (SD): Heat = 0x04, Off = 0x00, Cool = 0x02     Mode (SP): Constant = 0x04, Day/Night = 0x00, Night = 0x02
// (values from the MAME driver's INPUT_PORTS for tithermos)
#pragma once
#include <stdint.h>

namespace sw {

enum class Mode : uint8_t { Constant, DayNight, Night };
enum class System : uint8_t { Heat, Off, Cool };

struct Switches { Mode mode = Mode::Constant; System system = System::Heat; };

// Raw levels read while address 7 is selected (1 = high/pulled up, 0 = low/connected).
inline Switches decode(int io36, int io39, int io34, int io35) {
  Switches s;
  s.mode   = !io36 ? Mode::DayNight : !io39 ? Mode::Night : Mode::Constant;
  s.system = !io34 ? System::Off    : !io35 ? System::Cool : System::Heat;
  return s;
}

inline uint8_t rom_k_for_system_row(System s) { return s == System::Heat ? 0x04 : s == System::Cool ? 0x02 : 0x00; }
inline uint8_t rom_k_for_mode_row(Mode m)     { return m == Mode::Constant ? 0x04 : m == Mode::Night ? 0x02 : 0x00; }

// A slide switch passes through an open position between detents (Day/Night -> Night reads as
// 'Constant' for a moment), so only accept a new reading after it has been identical N scans in a row.
class Debounce {
 public:
  explicit Debounce(uint8_t n = 5) : need_(n) {}
  const Switches &update(const Switches &raw) {
    if (raw.mode == cand_.mode && raw.system == cand_.system) { if (count_ < 255) count_++; }
    else { cand_ = raw; count_ = 1; }
    if (count_ >= need_) stable_ = cand_;
    return stable_;
  }
  const Switches &stable() const { return stable_; }
 private:
  uint8_t need_, count_ = 0; Switches cand_, stable_;
};

}  // namespace sw
