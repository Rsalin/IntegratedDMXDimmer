/*
 * dmx_rx.h - DMX512 receiver (EUSART1) and address DIP switch
 */
#ifndef DMX_RX_H
#define DMX_RX_H

#include "board_config.h"

#define DMX_CHANNELS    512

typedef enum
{
    DATA_RX_INVALID,
    DATA_RX_VALID
} rx_valid_t;

extern volatile unsigned char dmx_data[NUM_CHANNELS];   // Slots starting at 'address'
extern volatile unsigned int address;                   // 0 = first slot after the start code
extern volatile rx_valid_t rx_valid;                    // Cleared if no frame within DMX_TIMEOUT_S

void dmx_init(void);
void dmx_isr(void);             // Call from the low priority ISR

void address_init(void);
unsigned int read_address(void);

#endif
