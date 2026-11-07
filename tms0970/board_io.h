// Pin map and HVAC output driver for the TI thermostat replacement board.
// Derived from the board schematic (74HC00 lockout + MOC3063 + BT134 stage).
//
//   heat  ON  <=>  IO4 = 1 && IO19 = 1     (U7A = NAND(IO19, IO4), LED/optotriac driven active-low)
//   cool  ON  <=>  IO4 = 1 && IO19 = 0     (U7B = NAND(!IO19, IO4))
//   fan   ON  <=>  IO2  = 0                (two inversions: net output == IO2, active-low drive)
//   humid ON  <=>  IO27 = 0                (same structure as fan)
// Heat and cool share IO4/IO19, so the hardware can never turn both on.
#pragma once
#include <stdint.h>

namespace board {

constexpr uint8_t PIN_MAX_LOAD = 5, PIN_MAX_CLK = 18, PIN_MAX_DIN = 23;     // MAX7219 (VSPI)
constexpr uint8_t PIN_SCAN_ADDR[3] = {25, 33, 32};                           // 74HC138 A0,A1,A2
constexpr uint8_t PIN_K_IN[4] = {36, 39, 34, 35};                            // data inputs; K bit order TBD
constexpr uint8_t PIN_FAN_SWITCH = 26;                                       // high = fan On, low = Auto
constexpr uint8_t PIN_HVAC_ENABLE = 4, PIN_HEAT_SELECT = 19;
constexpr uint8_t PIN_FAN = 2, PIN_HUMID = 27;

// Fan slide switch on IO26 (measured): 'On' = tied to 3.3 V, 'Auto' = tied to ground. No 'Off' position.
inline bool fan_switch_is_on(int io26_level) { return io26_level != 0; }
// Fan runs if the switch says On, or in Auto whenever heat/cool is being called (same as the original triac wiring).
inline bool fan_wanted(int io26_level, bool hvac_call) { return fan_switch_is_on(io26_level) || hvac_call; }

struct Outputs { bool heat = false, cool = false, fan = false, humid = false; };

struct PinLevels { uint8_t enable, heat_select, fan_pin, humid_pin; };

// Pure function so it can be unit-tested on the host.
inline PinLevels levels_for(const Outputs &o) {
  bool heat = o.heat, cool = o.cool;
  if (heat && cool) heat = cool = false;           // software guard on top of the hardware lockout
  PinLevels p;
  p.enable = (heat || cool) ? 1 : 0;
  p.heat_select = heat ? 1 : 0;
  p.fan_pin = o.fan ? 0 : 1;                        // active-low
  p.humid_pin = o.humid ? 0 : 1;                    // active-low
  return p;
}

#ifdef ARDUINO
// Call FIRST in setup(): preload the safe (all off) levels before switching pins to output,
// so the pins never glitch low.
inline void outputs_begin() {
  PinLevels off = levels_for(Outputs{});
  const uint8_t pins[4] = {PIN_HVAC_ENABLE, PIN_HEAT_SELECT, PIN_FAN, PIN_HUMID};
  const uint8_t lv[4] = {off.enable, off.heat_select, off.fan_pin, off.humid_pin};
  for (int i = 0; i < 4; i++) { digitalWrite(pins[i], lv[i]); pinMode(pins[i], OUTPUT); }
}
inline void outputs_apply(const Outputs &o) {
  PinLevels p = levels_for(o);
  // drop the enable first when changing heat<->cool so there is no instant with the wrong select
  digitalWrite(PIN_HVAC_ENABLE, 0);
  digitalWrite(PIN_HEAT_SELECT, p.heat_select);
  digitalWrite(PIN_HVAC_ENABLE, p.enable);
  digitalWrite(PIN_FAN, p.fan_pin);
  digitalWrite(PIN_HUMID, p.humid_pin);
}
#endif
}  // namespace board
