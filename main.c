/*
 * main.c - 8 channel DMX phase-control dimmer
 */
#include "board_config.h"
#include "config_bits.h"
#include "dimmer.h"
#include "dmx_rx.h"

static unsigned char channels_data[NUM_CHANNELS];

#define NOSC_EXTOSC_4PLL   0b010    // OSCCON1.NOSC

/* EXTOSC x4 PLL; wait until the crystal and the PLL are ready */
static void clock_init(void)
{
    OSCCON1 = (NOSC_EXTOSC_4PLL << _OSCCON1_NOSC_POSN) | (0b0000 << _OSCCON1_NDIV_POSN);
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
#ifdef TEST_FIXED_LEVEL
        channels_data[i] = TEST_FIXED_LEVEL;
#else
        if (rx_valid == DATA_RX_VALID && dmx_address < (DMX_CHANNELS - i))
            channels_data[i] = dmx_data[i];
        else
            channels_data[i] = 0;
#endif
    }
}

void main(void)
{
    unsigned int new_address;

    clock_init();
    dimmer_init(channels_data, NUM_CHANNELS);
    dmx_init();
    dmx_address_init();
    dmx_address = dmx_address_read();

#ifdef TEST_ALL_ON
    /* Hardware test: all triac gates on permanently, no interrupts */
#define X(p,n)  REG(LAT,p) |= MASK(n);
    OUTPUT_PINS(X)
#undef X
    while (1);
#endif

    INTCONbits.IPEN = 1;    // High/low priority vectors
    INTCONbits.GIEL = 1;
    INTCONbits.GIEH = 1;

    while (1)
    {
        new_address = dmx_address_read();
        if (new_address != dmx_address)
        {
            INTCONbits.GIEL = 0;    // 16 bit, also used by the DMX ISR
            dmx_address = new_address;
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
