/*
 * File:   config.h
 * Author: josefe
 *
 * Created on 25 de junio de 2017, 8:13
 */

#ifndef BUILD_CONFIG_H
#define	BUILD_CONFIG_H

#include <xc.h>

//#define debug

/*
 * Clock: 16 MHz crystal (HS) x 4 PLL -> Fosc = 64 MHz, Fcy = Fosc/4 = 16 MHz
 * Configuration bits are in config.h (included only from main.c)
 */
#define _XTAL_FREQ 64000000

#define NUM_CHANNELS 8

/*
 * Test mode jumper (RB5 -> RB6). Not available on the PIC18F46Q10 board:
 * RB5 is not connected and RB6 is ICSPCLK. Set to 1 only on boards that have it.
 */
#define TEST_MODE_JUMPER 0

#endif

