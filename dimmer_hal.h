/*
 * File:   dimmer_hal.h
 * Author: Rober
 *
 * Created on 17 April 2017, 10:49
 */

#ifndef DIMMER_HAL_H
#define	DIMMER_HAL_H

#include <xc.h> // include processor files - each processor file is guarded. 
#include "build_config.h"

//#define no_inline

#ifdef no_inline
    #define inline
#endif

#ifndef NUM_CHANNELS
    #define NUM_CHANNELS 8
#endif

#ifdef debug
#define debug_pin PORTDbits.RD0
#define debug_tris TRISDbits.TRISD0
#endif

/*Interruption flags*/
#define flagZC PIR0bits.INT0IF
#define flagT0 INTCONbits.TMR0IF
#define flagDMX PIR1bits.RCIF

/*Firing timer registers*/
#define firing_timer_count              TMR0L
#define flag_firing_timer               INTCONbits.TMR0IF
#define interrupt_enable_firing_timer   INTCONbits.TMR0IE
#define priority_firing_timer           INTCON2bits.TMR0IP
#define firing_enabled                  1
#define firing_disabled                 0
#define firing_high_priority            1
#define PRELOAD                         0x26 //Maybe deprecated

/*Zero crossing registers*/
#define trisZC                          TRISE
#define bitZC                           2
#define interrupt_enableZC              PIE0bits.INT0IE
#define interrupt_edgeZC                INTCONbits.INT0EDG
#define ZC_enabled                      1
#define ZC_rising_edge                  1
#define ZC_falling_edge                 0

/*Channel IO ports addresses*/
/*Some compilations are deprecated*/
volatile unsigned char* output_channels_addresses[NUM_CHANNELS] = {&PORTB, &PORTB, &PORTA, &PORTA, &PORTC, &PORTC, &PORTD, &PORTC};
volatile unsigned char* output_channels_tris[NUM_CHANNELS] = {&TRISB, &TRISB, &TRISA, &TRISA, &TRISC, &TRISC, &TRISD, &TRISC};
char  output_channels_bits[NUM_CHANNELS] = {3, 4, 2, 3, 5, 4, 3, 3};


//ch0 - RB3
//ch1 - RB4
//ch2 - RA2
//ch3 - RA3
//ch4 - RC5
//ch5 - RC4
//ch6 - RD3
//ch7 - RC3
/*HAL functions*/

/*Zero crossing*/
void zc_init(void);
inline char zc_check_flag(void);
inline void zc_clear_flag(void);
inline void zc_set_edge_direction(char direction);

/*Firing slot timer*/
void firing_timer_init(void);
inline void firing_timer_enable(void);
inline void firing_timer_disable(void);
inline void firing_timer_reset(void);
inline char firing_timer_check_flag(void);
inline void firing_timer_clear_flag(void);
inline void firing_timer_update_period(char period);
inline void firing_timer_reset_period(void);

/*Freq measuring timer*/
inline void freq_measuring_timer_init(void);
inline void freq_measuring_timer_restart (void);
inline unsigned char freq_measuring_timer_freeze(void);
inline char freq_measuring_timer_check_flag(void);
inline void freq_measuring_timer_clear_flag(void);

/*IO pins*/
void channels_init (void);
inline void turn_all_off(void);
inline void fire_all(unsigned char count, unsigned char* fire_tresholds);

#endif	/* DIMMER_HAL_H */

