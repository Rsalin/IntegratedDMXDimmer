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

/* ---- Clock: 12 MHz crystal (HS) x4 PLL = 48 MHz (set in config_bits.h) ---- */
#define XTAL_HZ         12000000UL
#define _XTAL_FREQ      48000000UL
#define FCY_HZ          (_XTAL_FREQ / 4)

/* ---- Outputs: X(port, bit), in channel order ch0..chN ---- */
#define OUTPUT_PINS(X)  X(B,3) X(B,4) X(A,2) X(A,3) X(C,5) X(C,4) X(D,3) X(C,3)
#define NUM_CHANNELS    8

/* ---- Zero crossing input (interrupt-on-change pin) ---- */
#define ZC_PORT         E
#define ZC_PIN          2

/* ---- DMX RX (EUSART1) ---- */
#define DMX_RX_PORT     C
#define DMX_RX_PIN      1
#define DMX_RX_PPS      0x11    /* RX1PPS value: (port index A=0,B=1,C=2.. << 3) | pin */
#define DMX_BAUD        250000UL
#define DMX_TIMEOUT_S   1       /* no valid frame for this long -> outputs off (max ~1) */

/* ---- DMX address DIP switch: X(port, bit), from address bit0 up ---- */
#define ADDR_PINS(X)    X(C,6) X(C,7) X(D,4) X(D,5) X(D,6) X(D,7) X(B,0) X(B,1) X(B,2)

/* ---- Timer setup (depends on Fcy) ----
 * Timer1 measures the half mains cycle; Timer3 must tick at HALF Timer1's rate
 * so that a period of TMR1H gives 128 slots. Timer1 must not overflow within
 * a half cycle at the lowest mains frequency: 65536*T1_DIV/Fcy > 10 ms.
 * Fcy = 12 MHz: T1 = /4 (3 MHz, overflow 21.8 ms), T3 = /8 (1.5 MHz). */
#define T1_PRESCALER    2       /* T1CON.CKPS: 0=1:1 1=1:2 2=1:4 3=1:8 */
#define T3_PRESCALER    3       /* T3CON.CKPS: must be T1_PRESCALER + 1 */

/* Timer0: Fcy / 256, 16 bit */
#define TMR0_RELOAD     (65536UL - (FCY_HZ / 256UL) * DMX_TIMEOUT_S)

/* ---- Helpers (do not edit) ---- */
#define CAT2_(a,b)      a##b
#define CAT3_(a,b,c)    a##b##c
#define REG(a,b)        CAT2_(a,b)          /* REG(LAT,B)    -> LATB */
#define REG3(a,b,c)     CAT3_(a,b,c)        /* REG3(IOC,E,P) -> IOCEP */
#define MASK(n)         ((unsigned char)(1u << (n)))

#endif
