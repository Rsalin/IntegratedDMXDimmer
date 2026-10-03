# DMX dimmer, PIC18F46Q10 (12 MHz crystal x4 PLL = 48 MHz, Fcy = 12 MHz)

Updated 2026-10-03. Builds without errors (XC8 3.10, PIC18F-Q_DFP 1.24.430, `-O0`): 2.7 KB flash, 58 B RAM. **Not tested on hardware.**

## Files
| File | Content |
|---|---|
| `board_config.h` | Everything board/PIC dependent: clock, output pins, ZC pin, DMX RX pin and PPS, DIP address pins, Timer1/3 prescalers, Timer0 reload |
| `config_bits.h` | Configuration words (only included from `main.c`) |
| `main.c` | Clock init, main loop, ISR vectors |
| `dimmer.c/.h` | Phase control (ZC, Timer1, Timer3+CCP1, outputs) |
| `dmx_rx.c/.h` | EUSART1 DMX receiver, Timer0 timeout, DIP address |

Fixed in code (same for the PIC18F4xQ10 family): EUSART1, Timer0/1/3, CCP1, IOC. Pins are X-macro lists in `board_config.h`; the code uses LATx/TRISx/ANSELx/PORTx by token pasting.

## Timing (Fcy = 12 MHz)
Timer1 = Fcy/4 (3 MHz, overflow 21.8 ms), Timer3 = Fcy/8 (1.5 MHz, half of Timer1, 128 slots), Timer0 = Fcy/256 with reload `65536 - 46875 = 0x48E5` (1 s), EUSART `SP1BRG = 47` (250 kbaud). If Fosc changes, adjust prescalers in `board_config.h` (T3 must be half of T1's rate).

## Hardware (U17 = PIC18F46Q10, TQFP44)
Outputs ch0..7: RB3, RB4, RA2, RA3, RC5, RC4, RD3, RC3. ZC: RE2 (IOC). DMX RX: RC1. DIP bit0..8: RC6, RC7, RD4-7, RB0-2. MCLR: RE3. No test jumper, no analog inputs (the old test mode and ADC fader code were removed; see git history before the restructuring).

## Pending (hardware)
1. Check the clock with a scope (12 MHz crystal, HS, PLL): the config requests EXTOSC x4 PLL; `clock_init()` waits for ORDY/EXTOR/PLLR.
2. ZC polarity: falling edge starts firing, rising edge is the zero crossing (as in the 18F4550 board).
3. DMX reception on RC1 and address mapping. DIP is read non-inverted (ON = 1), no pull-ups enabled; invert in `read_address()` or enable WPUx if needed.
4. BOR level 2.70 V chosen without knowing VCC.
5. Values taken from datasheet knowledge, not verified against the PDF: CCP mode `0b1010`, `C1TSEL = 0b10`, `T0CS = 0b010`, `T1CLK/T3CLK.CS = 0b0001`, `RX1PPS = 0x11` for RC1.
6. Timer1 resolution is now ~117 counts per 10 ms (was ~156), so the slot period has slightly coarser rounding.

Original working 18F4550 code: commit `1505473` on `master`.
