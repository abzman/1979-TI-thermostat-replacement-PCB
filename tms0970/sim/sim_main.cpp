#include "Arduino.h"
#include <vector>
uint64_t sim_now_us = 0; int sim_addr_pin[3] = {0, 0, 0}; SerialStub Serial;
static const uint8_t ADDR_PINS[3] = {25, 33, 32}, DATA_PINS[4] = {36, 39, 34, 35};
// ground truth: key name -> (addr, bit) in the order the sketch asks: 0,5,9,1,Time/Clock,Temp/Actual,8,7
struct Key { int addr, bit; };
static const Key TRUTH[8] = {{2,0},{2,1},{4,0},{6,1},{6,3},{6,2},{5,0},{3,0}};
// press schedule (absolute sim time in ms): list of keys held together, each with a start and duration
struct Press { int startMs, durMs; std::vector<int> keys; };
static std::vector<Press> sched = {
  {2500, 600, {0}},          // key '0'
  {4000, 600, {0}},          // '0' again when asked for '5'  -> must be rejected as duplicate
  {5500, 600, {1}},          // '5'
  {7000, 600, {1, 2}},       // two keys at once when asked for '9' -> must be rejected, retry
  {8500, 600, {2}},          // '9'
  {10000, 600, {3}}, {11500, 600, {4}}, {13000, 600, {5}}, {14500, 600, {6}}, {16000, 600, {7}}};
static bool pressed(int keyIdx) {
  uint64_t ms = sim_now_us / 1000;
  for (auto &p : sched) {
    if (ms < (uint64_t)p.startMs || ms >= (uint64_t)(p.startMs + p.durMs)) continue;
    for (int k : p.keys) if (k == keyIdx) {
      if (ms - p.startMs < 8) return ((sim_now_us / 400) & 1);   // contact bounce in the first 8 ms
      return true;
    }
  }
  return false;
}
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t v) { for (int i = 0; i < 3; i++) if (pin == ADDR_PINS[i]) sim_addr_pin[i] = v; }
int digitalRead(uint8_t pin) {
  int addr = sim_addr_pin[0] | sim_addr_pin[1] << 1 | sim_addr_pin[2] << 2;
  int bit = -1; for (int i = 0; i < 4; i++) if (pin == DATA_PINS[i]) bit = i;
  if (addr == 7 && bit == 0) return 0;                 // Mode switch in Day/Night: IO36 pulled low on address 7
  for (int k = 0; k < 8; k++) if (TRUTH[k].addr == addr && TRUTH[k].bit == bit && pressed(k)) return 0;
  return 1;                                            // pull-up
}
extern void setup(); extern void loop();
int main() { setup(); loop(); printf("[sim finished at %.1f s]\n", sim_now_us / 1e6); }
