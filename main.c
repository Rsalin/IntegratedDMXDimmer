/*
 * main.c - 8 channel DMX phase-control dimmer
 */
#include "board_config.h"
#include "config_bits.h"
#include "dimmer.h"
#include "dmx_rx.h"

static unsigned char channels_data[NUM_CHANNELS];

/* EXTOSC x4 PLL; wait until the crystal and the PLL are ready */
static void clock_init(void)
{
    OSCCON1 = (0b010 << _OSCCON1_NOSC_POSN) | (0b0000 << _OSCCON1_NDIV_POSN);
    while (!OSCCON3bits.ORDY);
    while (!OSCSTATbits.EXTOR);
    while (!OSCSTATbits.PLLR);
}

/* DMX data -> dimming values. No valid DMX: everything off */
static void process_channels(void)
{
    unsigned char i;
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        if (rx_valid == DATA_RX_VALID && address < (DMX_CHANNELS - i))
            channels_data[i] = dmx_data[i];
        else
            channels_data[i] = 0;
    }
}

void main(void)
{
    unsigned int new_address;

    clock_init();
    dimmer_init(channels_data, NUM_CHANNELS);
    dmx_init();
    address_init();
    address = read_address();

    INTCONbits.IPEN = 1;    // High/low priority vectors
    INTCONbits.GIEL = 1;
    INTCONbits.GIEH = 1;

    while (1)
    {
        new_address = read_address();
        if (new_address != address)
        {
            INTCONbits.GIEL = 0;    // 16 bit, also used by the DMX ISR
            address = new_address;
            INTCONbits.GIEL = 1;
        }
        process_channels();
        dimmer_set_levels(channels_data, NUM_CHANNELS);
    }
}

void __interrupt(high_priority) isr_high(void)
{
    dimmer_isr();
}

void __interrupt(low_priority) isr_low(void)
{
    dmx_isr();
}
