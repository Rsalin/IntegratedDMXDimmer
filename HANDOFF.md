# DMX dimmer, PIC18F46Q10 (12 MHz crystal x4 PLL = 48 MHz, Fcy = 12 MHz)

Updated 2026-10-03 (ZC moved to RB5, fixed 50 Hz). Builds without errors (XC8 3.10, PIC18F-Q_DFP 1.24.430, `-O0`): 2.4 KB flash, 60 B RAM. **Not tested on hardware.**

## Files
| File | Content |
|---|---|
| `board_config.h` | Everything board/PIC dependent: clock, output pins, ZC pin, DMX RX pin and PPS, DIP address pins, Timer0 reload |
| `config_bits.h` | Configuration words (only included from `main.c`) |
| `main.c` | Clock init, main loop, ISR vectors |
| `dimmer.c/.h` | Phase control (ZC, Timer3+CCP1, outputs) |
| `dmx_rx.c/.h` | EUSART1 DMX receiver, Timer0 timeout, DIP address |

Fixed in code (same for the PIC18F4xQ10 family): EUSART1, Timer0/3, CCP1, IOC. Pins are X-macro lists in `board_config.h`; the code uses LATx/TRISx/ANSELx/PORTx by token pasting.

## Timing (Fcy = 12 MHz, fixed 50 Hz)
No mains frequency measurement. ZC (IOC, both edges): falling edge starts the window, rising edge turns everything off. Window = `SLOTS` (128) slots of `SLOT_TICKS` (937) Timer3 ticks (Fcy, free-running CCP1 compare, no drift). Assumes the ZC falling->rising window is a 10 ms half cycle; the firing table in `dimmer.c` is for 50 Hz. Timer0 = Fcy/256, reload from `DMX_TIMEOUT_S`; EUSART `SP1BRG = 47` (250 kbaud). `TEST_ALL_ON` in `board_config.h` (commented) keeps all triacs on for hardware tests.

## Hardware (U17 = PIC18F46Q10, TQFP44)
Outputs ch0..7: RB3, RB4, RA2, RA3, RC5, RC4, RD3, RC3. ZC: **RB5** (IOC), wired with a jumper from the ZC signal on RE2 (RE2 has no IOC on this chip; only RE3 does). DMX RX: RC1. DIP bit0..8: RC6, RC7, RD4-7, RB0-2. MCLR: RE3. No test jumper, no analog inputs (the old test mode and ADC fader code were removed; see git history before the restructuring).

## Pending (hardware)
1. Check the clock with a scope (12 MHz crystal, HS, PLL): the config requests EXTOSC x4 PLL; `clock_init()` waits for ORDY/EXTOR/PLLR.
2. ZC polarity/window: falling edge starts firing, rising edge is the zero crossing; check the low time is ~10 ms.
3. DMX reception on RC1 and address mapping. DIP is read non-inverted (ON = 1), no pull-ups enabled; invert in `read_address()` or enable WPUx if needed.
4. BOR level 2.70 V chosen without knowing VCC.
5. Values taken from datasheet knowledge, not verified against the PDF: CCP mode `0b1010`, `C1TSEL = 0b10`, `T0CS = 0b010`, `T3CLK.CS = 0b0001`, `RX1PPS = 0x11` for RC1.

Original working 18F4550 code: commit `1505473` on `master`.
