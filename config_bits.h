/*
 * config_bits.h - Configuration words. Include ONLY from main.c.
 * PIC18F46Q10, 16 MHz crystal on RA6/RA7, x4 PLL -> 64 MHz.
 */
#ifndef CONFIG_BITS_H
#define CONFIG_BITS_H

#pragma config FEXTOSC = HS         // Crystal > 8 MHz
#pragma config RSTOSC = EXTOSC_4PLL // Start with EXTOSC x4 PLL (16 -> 64 MHz)
#pragma config CLKOUTEN = OFF
#pragma config CSWEN = ON
#pragma config FCMEN = ON
#pragma config MCLRE = EXTMCLR      // RE3 is MCLR
#pragma config PWRTE = OFF
#pragma config LPBOREN = OFF
#pragma config BOREN = SBORDIS
#pragma config BORV = VBOR_270
#pragma config ZCD = OFF
#pragma config PPS1WAY = OFF
#pragma config STVREN = ON
#pragma config XINST = OFF
#pragma config WDTE = OFF
#pragma config WDTCPS = WDTCPS_31
#pragma config WDTCWS = WDTCWS_7
#pragma config WDTCCS = SC
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF
#pragma config WRTC = OFF
#pragma config WRTB = OFF
#pragma config WRTD = OFF
#pragma config SCANE = ON
#pragma config LVP = ON
#pragma config CP = OFF
#pragma config CPD = OFF
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF
#pragma config EBTRB = OFF

#endif
