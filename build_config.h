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

// CONFIG1L
#pragma config FEXTOSC = HS     // External Oscillator mode (HS > 4MHz)
#pragma config RSTOSC = EXTOSC_4PLL // Startup with EXTOSC and 4x PLL (8MHz crystal -> 32MHz Fosc)

// CONFIG1H
#pragma config CLKOUTEN = OFF   // Disable CLKOUT on OSC2 (frees up the pin)
#pragma config CSWEN = ON       // Clock Switch Enable (Allows OSCCON1 changes)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor (Good for debugging crystals)

#define _XTAL_FREQ 32000000

#define NUM_CHANNELS 8

#endif


