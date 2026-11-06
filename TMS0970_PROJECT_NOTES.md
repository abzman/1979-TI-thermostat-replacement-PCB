# TI Electronic Digital Thermostat (1979) — ESP32 replacement firmware: project notes

Status as of 2026-11-06. Everything here was produced by emulating the original TMS0970 ROM
on a PC; **none of the firmware has been run on the ESP32 yet.** Items verified in emulation are
marked *(emu)*; items inferred from MAME/patent or your descriptions are marked *(assumed)*.

---------------------------------------------------------------------------------------------

## 1. Where the project stands

| Area | State |
|---|---|
| Portable TMS0970 CPU core (runs the real ROM) | Done *(emu)*. Boots, keeps time, scans keys, drives the display |
| Display path (ROM scan -> integrator -> MAX7219 registers) | Done *(emu)*, digit order and segment mapping verified |
| Output pins (heat/cool/fan/humidifier polarity, 74HC00 lockout) | Done, host-tested against a simulation of the gate network |
| Fan / system / mode switch decoding | Done at the pin level; K-bit translation see 4.3 |
| Key-matrix mapping on real hardware | Waiting on the `keymap_probe` report |
| ESP32 `Board : tms0970::Io` class tying it all together | Not written yet |
| Temperature sensing | **Not solved the original way.** A reliable shortcut exists, see section 6 |
| NTP, MQTT / Home Assistant | Not started |

---------------------------------------------------------------------------------------------

## 2. Hardware facts established

### 2.1 U1 pinout (SPDIP-28, your board photo vs. native DIP names)

| Pin | Name | Pin | Name |
|---|---|---|---|
| 1 | R7 | 28 | R6 |
| 2 | R8 | 27 | R5 |
| 3 | R9 | 26 | R4 |
| 4 | VDD (negative, battery -) | 25 | R3 |
| 5 | K1 | 24 | R2 |
| 6 | K2 | 23 | NC |
| 7 | K4 | 22 | NC (R1 not bonded) |
| 8 | K8 | 21 | R0 |
| 9 | INIT (grounded) | 20 | VSS (ground, positive rail) |
| 10 | O7 = SP | 19 | CK |
| 11 | O6 = SG | 18 | O0 = SA |
| 12 | O5 = SF | 17 | O1 = SB |
| 13 | O4 = SE | 16 | O2 = SC |
| 14 | NC | 15 | O3 = SD |

The SPDIP figure labels the R pins D1..D9 with D7 at pin 1 and D5 at pin 27. My patent-derived
notes called the A/D reset "D8" and the triac gate "D6". One of those is off by one. **Confirm
by checking which pin the 2.2k resistor to the SG node actually leaves from: pin 1 means the
names in this document are right, pin 2 means everything labelled R7 here is really R8.**

### 2.2 K lines on your replacement board (traced by you)

| K line | U1 pin | ESP32 pin |
|---|---|---|
| K1 | 5 | IO35 |
| K2 | 6 | IO34 |
| K4 | 7 | IO39 |
| K8 | 8 | IO36 |

Earlier code assumed IO36=K1, IO39=K2, IO34=K4, IO35=K8. That order was wrong. Anything that
maps pin to K bit must use the table above. All K lines go to the matrix through diodes
(cathode at the chip). K8 has a 100k pull-down.

### 2.3 Strobe lines (unchanged)
O0..O6 = SA..SG, O7 = SP. The 74HC138 on your board decodes three ESP32 address pins
(25, 33, 32); data inputs are IO36, IO39, IO34, IO35 (input-only, external pull-ups).
Switches hang on 138 output 7.

### 2.4 Output pins
IO4 = HVAC enable, IO19 = heat(1)/cool(0) select (74HC00 lockout), fan IO2 active-low,
humidifier IO27 active-low, fan switch IO26 (high = On, low = Auto).

### 2.5 The analog sensing section (original board, partly traced)
* U3 = LM2901 quad comparator (open-collector outputs).
* R7 goes through 2.2k onto the SG node.
* K8 reaches the matrix through a diode and is pulled down by 100k.
* Sensor: one leg to ground, other leg into a series/parallel resistor network that feeds U3.
* Emulator result: at every K read (ROM 01C) the R outputs carry only R5 and R7. No digital
  code is output on R0..R10 for the comparator to compare against. So the comparator
  reference is not a DAC driven by R; most likely the ramp or divider is passive plus R7.
* Still to trace: U3 pin 3 (Vcc, from SB/SD/SP through diodes?), which output pin feeds K8,
  what is on the +/- inputs, any capacitor, and everything on the SG node.

---------------------------------------------------------------------------------------------

## 3. Original firmware: how the chip is used

### 3.1 Chip
TMS0970 (MP6061, TMC0910B): 1 KB ROM, 64 x 4-bit RAM (X = 2 bits, Y = 4 bits, address = X<<4|Y,
written RAM[x][y] below), 6-bit PC, one-level subroutine stack, PLAs: instruction, microinstruction,
output (opla), segment (spla). ROM file order is *address = page<<6 | pc* with PC as an LFSR,
so **never reorder the ROM file**. Reset vector is ROM 3C0. One call to `Core::step()`
is one instruction; about 41.7k instructions/second at 250 kHz.

### 3.2 I/O behaviour of the ROM (from emulation, MAME, patent US4298946)
* The display is scanned segment by segment: O carries one segment strobe, R0..R3 select
  which digits light. Integrate over ~50-100 ms to get a picture. R0 is the **rightmost** digit.
  O7 (SP) is a key strobe, not a segment.
* K1 is a 60 Hz reference, read when O & 0x8A (SB/SD/SP high).
* K8 is the A/D comparator result on the SD and SP strobes.
* R5 is the HVAC call output (triac gate). In emulation it sits high when idle and goes
  low once a call is asserted, after about 150-160 s (the ROM's cycle delay). *(emu; polarity
  on the real gate network not yet confirmed)*
* R7 is the A/D reset. It pulses about every 10.2 s, driven by a software timer, not by K8.
* Display auto-blanks about 15 s after the last key.

### 3.3 Key matrix (MAME, matches the emulated ROM)
| Strobe | K1 | K2 | K4 | K8 |
|---|---|---|---|---|
| SA | 0 | 5 | Temp PM2 | Time PM2 |
| SC | 9 | 4 | Temp PM1 | Time PM1 |
| SE | 8 | 3 | Temp AM2 | Time AM2 |
| SF | 7 | 2 | Temp AM1 | Time AM1 |
| SG | 6 | 1 | Temp/Actual | Time/Clock |

Setting the clock: Time/Clock, then digits, then AM (Time AM1) or PM (Time PM2).

System switch is read on SD, mode switch on SP. The emulated ROM responds to:
heat = K4 (0x04) on SD, cool = K2 (0x02), off = 0; mode constant = K4 on SP.
Fan switch is not in the K matrix.

---------------------------------------------------------------------------------------------

## 4. Code inventory (all in this folder / the tarball)

### 4.1 Core and tables
| File | What it is |
|---|---|
| `tms0970.h`, `tms0970.cpp` | Portable CPU core. `Io` interface: `read_k()`, `write_o()`, `write_r()`. `Core` has `reset()`, `step()`, `o()`, `r()`, `ram(addr)`, `set_ram(addr, v)`, `fetched_address()` for debugging |
| `bake_tables.py` | Converts the Berkeley PLA files plus the ROM to C arrays. Usage: `python3 -I bake_tables.py tmc0910b tms0970_common2_instr.pla tms0970_tithermos_micro.pla tms0980_tithermos_output.pla tms0980_common2_segment.pla -o tithermos_tables.h` |
| `tithermos_tables.h` | Output of the above: `kRom`, `kIpla`, `kMpla`, `kOpla`, `kSpla` |

### 4.2 Board support (portable, host-tested)
| File | What it is |
|---|---|
| `display_integrator.h` | `DisplayIntegrator::sample(r, o)` every instruction, `snapshot(out[4])` every ~100 ms gives four segment bytes |
| `max7219_display.h` | `from_rom_segments()` bit-reverses to MAX7219 order; `Display<Bus>` writes registers only when changed; Arduino SPI bus guarded by `ARDUINO` |
| `board_io.h` | Pin constants, `levels_for(Outputs)`, fan/humidifier polarity, lockout, `outputs_begin()/outputs_apply()` |
| `switches.h` | `sw::decode(io36, io39, io34, io35)` -> mode and system; debounce class |
| `test_display.cpp`, `test_board_io.cpp`, `test_switches.cpp` | Host tests |

### 4.3 Known issue in `switches.h` (fix before use)
`decode()` works on pin levels and assumed the old pin-to-K order. With the corrected
K mapping, what changes is only which IO pin is which K line. The recommended approach: the
ESP32 reads its own switch pins, decodes the state, and then **presents the K bits the ROM
expects** (section 3.3). The wiring of the original switches is irrelevant, because the ROM
never sees it. Update the names in `switches.h` and re-run `test_switches`.

### 4.4 Arduino
`keymap_probe/keymap_probe.ino` asks you to press eight keys, scans 138 addresses 0..6 against
data pins {36,39,34,35}, and prints a report between `=== KEYMAP REPORT` and `=== END OF REPORT`.
Pin names in the report are ESP32 pins, so the corrected K order above is applied afterwards.

### 4.5 Exploration tools (host only)
| File | Use |
|---|---|
| `script_run*.cpp` | Drive the ROM with a key script and print the display. `script_run6` adds `POKE`, `POKE_EVERY`, `TARGET`, `K8T`, `K8POL`, `K8EDGE`, and reports R5 duty |
| `poketest.cpp` | Standalone version of the RAM-poke test |
| `kprobe.cpp` | Histogram of (ROM address, O, R, K) at K reads |
| `ramdump.cpp`, `ramwatch.cpp` | Dump RAM, list writer addresses per RAM nibble |
| `r7trace.cpp`, `dyntrace.cpp`, `explore.cpp`, `adprobe.cpp` | Instruction traces and A/D experiments |
| `disasm.py`, `annotate.py`, `annotate2.py`, `listing.txt` | Static listing in ROM address order, trace annotation |

Build any tool: `g++ -std=c++17 -O2 tool.cpp tms0970.cpp -o tool`.
Example: `POKE=72 ./script_run6 "wait:20 temp-actual:0.6" 0` shows the display after pressing Temp/Actual.
The display prints digit0 first, so `[27  ]` reads as 72.

---------------------------------------------------------------------------------------------

## 5. Where things live in the original ROM (addresses as printed in `listing.txt`)

### 5.1 Code
| ROM address | What |
|---|---|
| 3C0 | Reset vector |
| 01C, 087, 0A3 | The only true K readers. 01C and 0A3 are `TKA`, 087 is `KNEZ 9` |
| 3CB | Output-bit **clear** routine (`CLA`, then `SETR` with A=F) |
| 3E3 | Output-bit **set** routine (`CLA`, then `SETR` with A=8). Both are followed by `TCY 6; TMA; TDO`. The R7 pulse comes from these, driven by a software timer |
| 135, 13A | `TCMIY` constants: initial temperature digits (tens=5, units=1, which is why the display starts at 51) |
| 1CF | `TAMZA` store into the temperature units digit (the A/D result update) |
| 00F | Writer of the scan-loop counter RAM[2][6] |

The writer addresses were found with `ramwatch`, which reports the address of the instruction that wrote each
nibble. Other writers may exist that the trace did not hit.

### 5.2 RAM (RAM[x][y], flat address 0x{x}{y})
| RAM | Flat | Meaning |
|---|---|---|
| RAM[3][E] | 0x3E | **Temperature units digit** (displayed). Written at 13A (init) and 1CF |
| RAM[3][F] | 0x3F | **Temperature tens digit** (displayed). Written at 135 (init) |
| RAM[1][A] | 0x1A | Decimal counter for the ~10 s software timer that pulses R7 |
| RAM[2][9] | 0x29 | Flag bits set with `SBIT` shortly before the R7 pulse |
| RAM[2][6] | 0x26 | Scan-loop counter (changes every pass) |
| RAM[1][6..8] | 0x16-0x18 | Vary with K8; writers: RAM[1][7] at 0C6/0CC, RAM[1][8] at 0ED/175 |
| RAM[2][D], RAM[2][F] | 0x2D, 0x2F | Also vary with K8; writers 036/1F1 and 004 |

Keep in mind the ROM uses RAM[3][F:E] as the temperature it both **displays** and **controls
on**. That is the hook used in section 6.

---------------------------------------------------------------------------------------------

## 6. Getting temperature working without reverse engineering the analog section

### 6.1 Recommended: write the temperature into ROM RAM from the ESP32 *(tested in emulation)*
Every so often, set the ROM's temperature digits to the BME280 reading in whole degrees F:

```cpp
// Call from the main loop, e.g. before each batch of core.step() calls.
void apply_temperature(tms0970::Core &cpu, int deg_f) {
  if (deg_f < 50) deg_f = 50;      // see caveats on range
  if (deg_f > 99) deg_f = 99;
  cpu.set_ram(0x3E, deg_f % 10);   // units
  cpu.set_ram(0x3F, deg_f / 10);   // tens
}
```

What the emulator showed (`script_run6`, K8 either absent or forced high, no A/D model):

* **Display:** after pressing Temp/Actual, poked values 55, 68, 72 and 85 displayed exactly.
  Without a poke it shows the init value 51.
* **Refresh rate:** results were identical when refreshed every instruction, every 200 instructions
  (about 5 ms), 4167 (0.1 s), 41667 (1 s) and 416667 (10 s, but the check came right after a poke).
  **Refresh at 1 Hz or faster**; every loop is easiest. A slower rate was not shown to be safe.
* **Control response:** with the default setpoint (somewhere between 60 and 75 F) and a 400 s run,
  R5 (the HVAC call) stays idle-high for temperature 95 and goes low after the cycle delay
  (~157 s) for temperature 45 in heat mode. In cool mode (K2 on SD) it is the opposite: low for
  75 and 95, idle for 45 and 60. With the system switch off it never goes low. Forcing K8 high does
  not change these results.

Why it works: the A/D result lands in the same two RAM nibbles the display and control logic read,
and nothing else depends on the analog measurement *(as far as the tests show)*.

### 6.2 Practical notes
* The ROM works in whole degrees, so round or smooth the BME280 (a moving average of a few
  seconds, then round). Calibrate the offset against a reference thermometer; the BME280 reads
  warm if it sits on the board.
* Put the reading behind a sensor interface so the BME280 can be swapped for another part
  without touching the ROM-facing code.
* The ROM's own hysteresis and 3-minute cycle delay then apply unchanged.
* Range is untested beyond 55-85 F for the display and 45-95 F for control. Clamp, and test the
  ends before relying on them (what the ROM does at 99 F, or below 50 F, is unknown).

### 6.3 Alternatives (if poking turns out to be insufficient)
1. **Patch the ROM.** Replace the instruction that stores the A/D result (1CF `TAMZA`) and the
   init writes at 135/13A so the ROM never overwrites the digits. This avoids the refresh
   requirement but modifies the ROM image (keep the original; patch in the tables header).
2. **Virtual comparator.** Make K8 high when the ROM's own displayed estimate is below a target
   (inverted polarity, `TARGET` and `K8POL=1` in `script_run6`). It converges for targets about
   51-60 F and stalls at 59-60 F above that, so it is only a partial model and not recommended.
3. **Model the real analog circuit** once the net list (section 2.5) is traced. This is the only way to
   reproduce the original calibration, and it needs the resistor values around U3, the
   capacitor value, and the sensor resistance at a known temperature.

### 6.4 What was tried and did not work
K8 as a fixed delay after the R7 edge, K8 as a gated integrator (counting only while SB/SD/SP is
strobed), each in both polarities, and K8 as a comparator against a target. None gave a stable
reading that tracks a setpoint across cycles. The comparator model came closest.

---------------------------------------------------------------------------------------------

## 7. Open items and risks

1. **Key matrix on hardware.** Run `keymap_probe` and send the report; it fills in the strobe
   address and data pin of each key. Then write the `Board` class.
2. **`Board : tms0970::Io` class (not written).** Key matrix scan, switch decode, K1 60 Hz
   synthesis, display via integrator -> MAX7219, outputs via `board_io.h`, temperature
   injection (6.1). A fixed fake temperature is fine to start.
3. **R5 polarity.** The emulator shows R5 going low for a call. Check on the real gate network
   whether low or high drives the triac stage before wiring it to IO4/IO19.
4. **R7 / D8 naming** (2.1).
5. **`switches.h`** K-order fix (4.3).
6. **IO2 boot behaviour**: IO2 is a strapping pin; check the fan state at power-up and keep the
   pin preloaded off before `pinMode`.
7. **Differential test** against MAME's trace of the same ROM once the board class exists.
8. **Display windows** occasionally catch a partial scan; use a longer window or require two
   consecutive snapshots to agree.
9. **NTP / MQTT.** Planned: set the clock either by injecting the key sequence (Time/Clock, digits,
   AM/PM) or by writing the ROM's clock RAM directly; Home Assistant MQTT climate entity with
   discovery; a policy for conflicts between the keypad and remote setpoint changes.

---------------------------------------------------------------------------------------------

## 8. References
* MAME `hh_tms1k.cpp` (tithermos driver), `tms0970.cpp`, `tms1k_base.cpp`, ROM and PLA files for tithermos.
* US patent 4,298,946 (the thermostat's A/D and key scanning).
* Your write-up: abzman2k.wordpress.com, "TI digital thermostat replacement board".
