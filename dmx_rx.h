

/*
 * File:
 * Author:
 * Comments:
 * Revision history:
 */

// This is a guard condition so that contents of this file are not included
// more than once.
#ifndef DMX_RX_H
#define	DMX_RX_H

#include <xc.h> // include processor files - each processor file is guarded.
#include "build_config.h"

#define TOTALCHANNELS   512
#define LOG2_512        9
#define ADDRESS_OFFSET  0

#ifndef NUM_CHANNELS
    #define NUM_CHANNELS 8
#endif

/*1 sec period
 65536-16.000.000/PRESCALER[256] = 3036 = 0x0BDC*/
#define USART_TIMEOUT_H 0x0B
#define USART_TIMEOUT_L 0xDC

typedef enum
{
    DATA_RX_INVALID,
    DATA_RX_VALID
}rx_valid_t;

extern volatile unsigned char TramaDMX[TOTALCHANNELS]; //Rx buffer
extern volatile unsigned int address;                  //Address variable
extern volatile rx_valid_t rx_valid;

void usart_timeout_timer_init(unsigned char low, unsigned char high);
inline void usart_timeout_reset(unsigned char low, unsigned char high);
inline void usart_timeout_isr(void);
void usart_config(void);
inline void  usart_isr(void);

void address_init(void);
unsigned int read_address(void);

#endif	/* DMX_RX_H */

