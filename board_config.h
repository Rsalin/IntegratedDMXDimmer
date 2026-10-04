/*
 * board_config.h - Everything that depends on the board / exact PIC.
 * To port to a similar PIC (PIC18F4xQ10 family) or to another PCB, edit only
 * this file and config_bits.h.
 *
 * Fixed in the code (same on all PIC18-Q): EUSART1, Timer0, Timer1, Timer3, CCP1, IOC.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <xc.h>

/* Hardware test: uncomment to keep all triacs always on (mains connected: careful) */
//#define TEST_ALL_ON

/* Hardware test: fixed dimming value on all channels, DMX ignored (64 = 25%).
 * Comment out to use DMX again. */
//#define TEST_FIXED_LEVEL    64

/* ---- Clock: 16 MHz crystal (HS) x4 PLL = 64 MHz (set in config_bits.h) ---- */
#define XTAL_HZ         16000000UL
#define _XTAL_FREQ      64000000UL
#define FCY_HZ          (_XTAL_FREQ / 4)

/* ---- Outputs: X(port, bit), in channel order ch0..chN ---- */
#define OUTPUT_PINS(X)  X(B,3) X(B,4) X(A,2) X(A,3) X(C,5) X(C,4) X(D,3) X(C,3)
#define NUM_CHANNELS    8

/* ---- Zero crossing input (interrupt-on-change pin) ---- */
#define ZC_PORT         B       /* needs interrupt-on-change (RE2 has none) */
#define ZC_PIN          5
/* The ZC signal is also wired to this pin: keep it high impedance (input, analog) */
#define ZC_ALT_PORT     E
#define ZC_ALT_PIN      2

/* ---- DMX RX (EUSART1) ---- */
#define DMX_RX_PORT     C
#define DMX_RX_PIN      1
#define DMX_RX_PPS      0x11    /* RX1PPS value: (port index A=0,B=1,C=2.. << 3) | pin */
#define DMX_BAUD        250000UL
#define DMX_TIMEOUT_S   1       /* no valid frame for this long -> outputs off (max ~1) */

/* ---- DMX address DIP switch: X(port, bit), from address bit0 up ---- */
#define ADDR_PINS(X)    X(C,6) X(C,7) X(D,4) X(D,5) X(D,6) X(D,7) X(B,0) X(B,1) X(B,2)

/* ---- Dimmer timing (Timer3 runs at Fcy, 1:1) ----
 * Fixed mains frequency. The firing window (ZC falling -> rising edge) is
 * assumed to be one half mains cycle, split in SLOTS = 256 slots, one per
 * firing table step (firing_table.h is calculated for 50 Hz, 1/256 steps). */
#define MAINS_HZ        50
#define SLOTS           256
#define SLOT_TICKS      (FCY_HZ / (2UL * MAINS_HZ * SLOTS))     /* 625 @ 16 MHz Fcy */

/* Timer0: Fcy / 256, 16 bit */
#define TMR0_RELOAD     (65536UL - (FCY_HZ / 256UL) * DMX_TIMEOUT_S)

/* ---- Helpers (do not edit) ---- */
#define CAT2_(a,b)      a##b
#define CAT3_(a,b,c)    a##b##c
#define REG(a,b)        CAT2_(a,b)          /* REG(LAT,B)    -> LATB */
#define REG3(a,b,c)     CAT3_(a,b,c)        /* REG3(IOC,E,P) -> IOCEP */
#define MASK(n)         ((unsigned char)(1u << (n)))

#endif
