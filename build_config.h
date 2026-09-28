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


// CONFIG2L
#pragma config MCLRE = INTMCLR  // LIBERA RE3: Ahora puedes usarlo para el cruce por cero
#pragma config PWRTE = OFF  // Power-up Timer Enable bit
#pragma config BOREN = ON

#define _XTAL_FREQ 48000000

#define NUM_CHANNELS 8

#endif


