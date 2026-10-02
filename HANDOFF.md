# Handoff: porting the 8-channel DMX dimmer from PIC18F4550 to PIC18F46Q10

Last updated: 2026-10-02

## 1. Current status

| Item | Status |
|---|---|
| Code changes for the Q10 port | Done |
| Build (XC8 v3.00, `-O0`, `-std=c99`, PIC18F-Q_DFP 1.24.430) | **Passes**, no errors. Uses 3438 B flash (5.2 %) and 582 B RAM (17.3 %) |
| Config words checked in the `.hex` | Yes: `AA FF FF F5 9F FF ...` (HS + 4xPLL, WDT off, PPS1WAY off, BOR 2.70 V) |
| Tested on hardware | **No.** Nothing has been flashed yet |
| Committed | Yes, on branch `claude_code_branch` (see section 2). Not pushed yet |

The code had never built for the Q10 before this work. The `.hex` in `dist/` was an old build.

## 2. Moving the work to the other machine

All the port work is committed on branch `claude_code_branch`. The commit includes:
- the 9 source files and `HANDOFF.md`;
- the MPLAB X project files `Makefile`, `nbproject/configurations.xml` and `nbproject/project.xml`. The stale MCC entries were removed from `configurations.xml`;
- the deletion of the old `mcc_generated_files/system/*` and of the old 18F4550 `.hex`;
- `.gitignore` (renamed from `gitignore`, which git was not using). It now ignores `build/`, `dist/`, `debug/`, the per-machine files in `nbproject/` (`private/`, `Makefile-*.mk`, `Makefile-genesis.properties`, `Package-*.bash`), the MPLAB logs, and the MCC files (`*.mc3`, `mcc-manifest-*.yml`), since MCC is no longer used.

Steps:
1. On this machine: `git push -u origin claude_code_branch`. The remote is `https://github.com/Rsalin/IntegratedDMXDimmer`. The branch `port_to_PIC18F46Q10` is at the older commit `4f37544`; merge into whichever branch you prefer.
2. On the other machine: clone, check out `claude_code_branch`, and open the folder as a project in MPLAB X. MPLAB X regenerates `nbproject/Makefile-*.mk` and `nbproject/private/` on the first build. If the PIC18F-Q_DFP 1.24.430 pack is missing, MPLAB X offers to download it.

## 3. Toolchain

- MPLAB X v6.20 (v6.10 is also installed on the current machine).
- XC8 **v3.00**.
- Device pack **PIC18F-Q_DFP 1.24.430** (the version the project uses). Version 1.28.451 is also installed in `~/.mchp_packs`.
- Programmer in the project: PICkit 3.
- Command-line build used for checking (from the project folder):

```
"C:/Program Files/Microchip/xc8/v3.00/bin/xc8-cc.exe" -mcpu=18F46Q10 \
  -mdfp="C:/Program Files/Microchip/MPLABX/v6.20/packs/Microchip/PIC18F-Q_DFP/1.24.430/xc8" \
  -O0 -std=c99 main.c dimmer.c dimmer_hal.c dmx_rx.c -o build/test.elf
```

Three warnings are expected, all for unused functions: `osc_test`, `get_fire_tresholds_buffer` and `firing_timer_reset_period`.

## 4. Hardware (board schematic, U17 = PIC18F46Q10-I/PT, TQFP44)

| Function | Pin | Notes |
|---|---|---|
| Crystal Y1 | RA6 / RA7 | 16 MHz, HS mode, x4 PLL = 64 MHz |
| Zero crossing (ZC) | **RE2** | Uses interrupt-on-change (IOC), not INT0 (see section 6) |
| ICSP MCLR | RE3 | Kept as MCLR (`MCLRE = EXTMCLR`) |
| DMX RX (EUSART1) | **RC1** | `RX1PPS = 0x11` |
| triac0..triac7 | RB3, RB4, RA2, RA3, RC5, RC4, RD3, RC3 | Channel order ch0..ch7 |
| addr0, addr1 | RC6, RC7 | DMX address DIP switch, 9 bits |
| addr2..addr5 | RD4..RD7 | |
| addr6..addr8 | RB0..RB2 | |
| ICSPCLK / ICSPDAT | RB6 / RB7 | |
| Not connected | RA0, RA1, RA4, RA5, RB5, RC0, RC2, RD0-RD2, RE0, RE1 | |

The board has no test-mode jumper (the old one was RB5 to RB6) and no analog inputs. `adc_buffer[]` is always 0.

## 5. Firmware architecture (unchanged from the 18F4550 version)

- `dimmer.c`: phase-control state machine, driven from the high-priority ISR. It measures the time from the ZC falling edge to the rising edge with **Timer1**. It then divides that window into **128 slots** using **Timer3 + CCP1** in compare mode, with the period equal to `TMR1H`. On each slot `fire_all()` sets the outputs whose threshold has been reached. On the ZC rising edge all outputs are turned off. If Timer1 overflows (mains lost), all outputs are turned off and the state goes back to IDLE.
- `dimmer_hal.c`: register-level access (ZC, Timer1, Timer3/CCP1, output pins).
- `dmx_rx.c`: EUSART1 at 250 kbaud, 9-bit, in the low-priority ISR. A state machine detects the break (FERR with data 0) and the start code, then stores `NUM_CHANNELS` slots starting at `address` in `TramaDMX[]`. **Timer0** is a 1 s timeout: if no full frame arrives in time, `rx_valid = DATA_RX_INVALID` and the outputs go to `adc_buffer` (0).
- `main.c`: initialisation, then a loop that reads the DIP address, copies DMX data to `channels_data[]`, and calls `set_fire_tresholds_buffer()`.
- Interrupt priorities: high = IOC (ZC), CCP1, TMR1. Low = RC1 (DMX), TMR0.

### Clock and timing (Fosc 64 MHz, Fcy 16 MHz)

| Timer | Source | Prescaler | Tick | Use |
|---|---|---|---|---|
| Timer1 | Fosc/4 (`T1CLK.CS = 0001`) | 1:4 | 4 MHz | Half-cycle measurement. Overflows at 16.4 ms. 10 ms gives `TMR1H` of about 156 |
| Timer3 | Fosc/4 | 1:8 | 2 MHz | Slot timebase. Half of Timer1's rate, so 128 slots per window |
| Timer0 | Fosc/4, 16-bit | 1:256 | 62.5 kHz | DMX timeout. Preload `0x0BDC` = 1 s |
| EUSART1 | BRG16 = 1, BRGH = 1 | `SP1BRG = 63` | | 250 kbaud exactly |

The ratio of instruction cycles per Timer3 tick is 8, the same as on the 18F4550. ISR latency therefore affects slot timing the same way as on the original board.

## 6. What was changed and why

Each item below was a real fault in the earlier port.

1. **The code did not compile.** `dimmer_hal.h` declared `char zc_isr()`; it should have been `zc_check_flag()`.
2. **ZC could not reach INT0.** On the Q10, `INT0PPS` is 4 bits and can only select PORTA or PORTB. The old value `0x23` was truncated to `0x03`, which is RA3 (triac3 output). ZC on RE2 now uses IOC: `IOCEP2`/`IOCEN2` select the edge, and `IOCEF2` is the flag. `IOCIF` is read-only and clears when `IOCEF2` is cleared.
3. **The watchdog was on.** The default config has `WDTE` on, which reset the chip about every 2 s. A full Q10 config block was added in `config.h` under `#if defined(_18F46Q10)`. It is included only from `main.c`. The `#pragma config` lines were removed from `build_config.h`, which every file includes.
4. **Timer1 clock.** The reset value of `T1CLK` selects the T1CKI pin (RC6, which is a DIP input). It is now set to Fosc/4.
5. **Timer1 flags.** The code used the Timer1 *gate* flags (`PIE5/PIR5.TMR1GIx`). It now uses `PIE4/PIR4/IPR4.TMR1Ix`.
6. **PPS.** `main` locked PPS and `usart_config` tried to unlock it again. With `PPS1WAY = ON` (default) the second unlock fails, so RX stayed on RC7. PPS is now set once, in `usart_config`, and `PPS1WAY = OFF`. The `GIE = 1` call in the middle of initialisation was removed.
7. **Timer0.** The prescaler was never set, so the "1 s" timeout lasted about 4 ms. The preload was also still the 12 MHz value. Now 1:256 with `0x0BDC`.
8. **ANSEL.** On the Q10 every pin resets as analog. In that state `PORTx` reads 0, so `PORTx |= mask` cleared the other channels on the same port, and digital inputs always read 0. Outputs now go through **LATx** tables, and `ANSEL` is cleared on every pin used: outputs, RE2, RC1 and the DIP pins.
9. **`address_init()` set RD3 (triac6) as an input**, so that channel never fired. It was rewritten for the new DIP pins.
10. **`firing_timer_init()` enabled the CCP1 interrupt and started Timer3 before ZC sync**, so triacs fired while in the IDLE state. Both are now enabled only by `firing_timer_enable()`.
11. **Global variables defined in headers** (`TramaDMX`, `address`, `rx_valid`, `firing_map`, the pin tables) were moved to their `.c` files, with `extern` in the headers. `rx_valid` now has a named type, `rx_valid_t`.
12. **DMX value 0 now never fires** (`NEVER_FIRE_SLOT = 255`). Before, it gave threshold 127, which could fire the triac just before the zero crossing.
13. `read_address()` was implemented for the new pins: bit0-1 = RC6-7, bit2-5 = RD4-7, bit6-8 = RB0-2. It is limited to 512, as in the original. In the main loop, `address` is written with `GIEL` disabled, because it is a 16-bit value that the DMX ISR reads.
14. `clock_init()` requests the EXTOSC x4 PLL clock (`OSCCON1 = 0x20`), then waits for `ORDY`, `EXTOR` and `PLLR`. `_XTAL_FREQ` is now 64000000 (it was 48000000).
15. Test mode is disabled with `TEST_MODE_JUMPER 0` in `build_config.h`, because the board has no jumper. The original code is still there, inside `#if`.
16. The `#include "pic18f46q10.h"` in `main.c` was removed; `xc.h` already includes it.

### Configuration words (in `config.h`)

`FEXTOSC = HS`, `RSTOSC = EXTOSC_4PLL`, `CLKOUTEN = OFF`, `CSWEN = ON`, `FCMEN = ON`, `MCLRE = EXTMCLR`, `PWRTE = OFF`, `LPBOREN = OFF`, `BOREN = SBORDIS`, `BORV = VBOR_270`, `ZCD = OFF`, `PPS1WAY = OFF`, `STVREN = ON`, `XINST = OFF`, `WDTE = OFF`, `LVP = ON`. All code and table-read protections are off.

## 7. Open items and things to verify

**To do on the hardware first:**
1. Flash the board and check the clock. Call `osc_test()` (it toggles RB3 in a loop) and measure the frequency with a scope or counter. RB3 is the triac0 gate, so do this **with mains disconnected**. Each loop iteration takes a few instruction cycles (Fcy = 16 MHz), so check the frequency against the generated assembly.
2. Check the ZC signal on RE2. The firmware assumes the same polarity as the old board: the falling edge starts the firing window and the rising edge marks the zero crossing. If the dimming curve looks inverted or wrong, the ZC polarity is the first thing to check.
3. Check DMX reception: valid frames on RC1, and the channel mapping for different DIP addresses.

**Still to decide or confirm:**
- **DIP switch polarity.** The DIP is read non-inverted (switch ON = 1) with no internal pull-ups, as in the 18F4550 code. The schematic excerpt does not show the pull resistors. If the switches pull to GND with pull-ups, invert the bits in `read_address()` (`dmx_rx.c`), or enable `WPUx` if there are no external resistors.
- **Address meaning.** `address = 0` means the first DMX slot after the start code (DMX channel 1), as in the original.
- **BOR level.** 2.70 V was chosen without knowing the board's VCC. Adjust `BORV` if needed.
- **Values not taken from the device files.** Register and bit names, and reset values, come from the installed device header and the EDC (`PIC18F46Q10.PIC`). The following field values come from datasheet knowledge and have not yet been checked against the PDF: CCP mode `0b1010` (compare, interrupt on match), `CCPTMRS.C1TSEL = 0b10` (Timer3), `T0CS = 0b010` (Fosc/4), `T1CLK/T3CLK.CS = 0b0001` (Fosc/4), and the `RX1PPS` encoding (`0x11` = RC1).
- **Possible improvement, not applied.** CCP mode `0b1011` clears TMR3 on match and would give exact slot periods without the reset in the ISR. It was not applied because it changes the timing at the end of the half cycle compared with the version known to work.
- **ADC.** The 18F4550 version read local faders into `adc_buffer[]`. The new board has no analog inputs, so that code was not ported.

## 8. Source of truth

- Original working 18F4550 code: commit `1505473` ("Added hardware test features."), on `master`.
- Earlier port attempts: `fe25894`, `49df454`, `bad0873`, `4f37544`.
- The old MCC configuration (now deleted) can be seen with `git show fe25894:mcc_generated_files/system/src/pins.c`.
- Device files used as reference (current machine):
  - `C:\Users\Rober\.mchp_packs\Microchip\PIC18F-Q_DFP\1.28.451\xc8\pic\include\proc\pic18f46q10.h`
  - `C:\Users\Rober\.mchp_packs\Microchip\PIC18F-Q_DFP\1.28.451\edc\PIC18F46Q10.PIC` (register reset values and field widths)
  - `...\xc8\pic\dat\cfgdata\18f46q10.cfgdata` (config bit names)
