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
#define debug_pin LATDbits.LATD0
#define debug_tris TRISDbits.TRISD0
#endif

/*
 * Zero crossing registers (PIC18F46Q10)
 * ZC is on RE2. INT0 can only be mapped (PPS) to PORTA/PORTB, so the
 * interrupt-on-change (IOC) of RE2 is used instead, selecting the edge
 * with IOCEP/IOCEN. IOCIF is read-only: it is cleared by clearing IOCEF2.
 */
#define trisZC                  TRISEbits.TRISE2
#define anselZC                 ANSELEbits.ANSELE2
#define ioc_positive_edgeZC     IOCEPbits.IOCEP2
#define ioc_negative_edgeZC     IOCENbits.IOCEN2
#define flagZC                  IOCEFbits.IOCEF2
#define interrupt_enableZC      PIE0bits.IOCIE
#define priorityZC              IPR0bits.IOCIP
#define ZC_enabled              1
#define ZC_rising_edge          1
#define ZC_falling_edge         0

/*Channel IO ports addresses*/
//ch0 - RB3
//ch1 - RB4
//ch2 - RA2
//ch3 - RA3
//ch4 - RC5
//ch5 - RC4
//ch6 - RD3
//ch7 - RC3
extern volatile unsigned char* const output_channels_latches[NUM_CHANNELS];
extern volatile unsigned char* const output_channels_tris[NUM_CHANNELS];
extern volatile unsigned char* const output_channels_ansel[NUM_CHANNELS];
extern const unsigned char output_channels_masks[NUM_CHANNELS];

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
inline void firing_timer_update_period(unsigned char period);
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

