/*
 * dmx_rx.c - DMX512 receiver (EUSART1, 250 kbaud 9 bit) and address DIP switch.
 * A frame is: break (FERR with data 0), start code 0, then the slots.
 * Timer0 is a timeout: if no complete frame arrives, rx_valid is cleared.
 */
#include "dmx_rx.h"

/* PIC18-Q register values (datasheet DS40001996D) */
#define T0CS_FOSC_4     0b010       // Timer0 clock source: Fosc/4
#define T0CKPS_1_256    0b1000      // Timer0 prescaler 1:256

typedef enum
{
    ST_WAIT_BREAK,
    ST_WAIT_BYTE,       // After an error: wait for a good byte, then look for a break
    ST_WAIT_START,
    ST_RECEIVING
} dmx_state_t;

volatile unsigned char dmx_data[NUM_CHANNELS];
volatile unsigned int dmx_address;
volatile rx_valid_t rx_valid;

static volatile dmx_state_t state;
static unsigned int slot_index;
static unsigned char data_index;

/* Restarts the timeout and validates the data */
static void timeout_restart(void)
{
    TMR0H = (unsigned char)(TMR0_RELOAD >> 8);
    TMR0L = (unsigned char)TMR0_RELOAD;
    rx_valid = DATA_RX_VALID;
    T0CON0bits.T0EN = 1;
    PIR0bits.TMR0IF = 0;
    PIE0bits.TMR0IE = 1;
}

void dmx_init(void)
{
    /* Timeout timer: Timer0 16 bit, Fcy, 1:256, low priority */
    T0CON0 = 0;
    T0CON0bits.T016BIT = 1;
    T0CON1bits.T0CS = T0CS_FOSC_4;
    T0CON1bits.T0ASYNC = 0;
    T0CON1bits.T0CKPS = T0CKPS_1_256;
    IPR0bits.TMR0IP = 0;
    timeout_restart();

    /* RX pin */
    REG(TRIS,DMX_RX_PORT) |= MASK(DMX_RX_PIN);
    REG(ANSEL,DMX_RX_PORT) &= (unsigned char)~MASK(DMX_RX_PIN);
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 0;
    RX1PPS = DMX_RX_PPS;
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1;

    /* EUSART1: async, 9 bit, continuous receive */
    TX1STAbits.BRGH = 1;
    BAUD1CONbits.BRG16 = 1;
    TX1STAbits.SYNC = 0;
    SP1BRG = (unsigned int)(_XTAL_FREQ / (4UL * DMX_BAUD)) - 1;
    RC1STAbits.RX9 = 1;
    RC1STAbits.SREN = 0;
    RC1STAbits.ADDEN = 0;
    RC1STAbits.SPEN = 1;
    RC1STAbits.CREN = 1;

    state = ST_WAIT_BREAK;
    IPR3bits.RC1IP = 0;
    PIE3bits.RC1IE = 1;
}

static void rx_isr(void)
{
    unsigned char ferr, data;

    ferr = RC1STAbits.FERR;     // Of the byte on top of the FIFO: read before RC1REG
    data = RC1REG;

    if (RC1STAbits.OERR)
    {
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
        state = ST_WAIT_BYTE;
        return;
    }

    switch (state)
    {
    case ST_WAIT_BYTE:
        if (!ferr)
            state = ST_WAIT_BREAK;
        break;

    case ST_WAIT_BREAK:
        if (ferr && !data)
            state = ST_WAIT_START;
        break;

    case ST_WAIT_START:
        if (ferr)
            state = ST_WAIT_BYTE;
        else if (!data)
        {
            slot_index = 0;
            data_index = 0;
            state = ST_RECEIVING;
        }
        else
            state = ST_WAIT_BREAK;
        break;

    case ST_RECEIVING:
        if (ferr)
        {
            state = data ? ST_WAIT_BYTE : ST_WAIT_START;
            break;
        }
        if (slot_index >= dmx_address)
            dmx_data[data_index++] = data;
        slot_index++;
        if (data_index >= NUM_CHANNELS || slot_index >= DMX_CHANNELS)
        {
            state = ST_WAIT_BREAK;
            timeout_restart();
        }
        break;
    }
}

void dmx_isr(void)
{
    if (PIR3bits.RC1IF)
        rx_isr();

    if (PIE0bits.TMR0IE && PIR0bits.TMR0IF)
    {
        rx_valid = DATA_RX_INVALID;
        T0CON0bits.T0EN = 0;
        PIE0bits.TMR0IE = 0;
        PIR0bits.TMR0IF = 0;
    }
}

/* ---- Address DIP switch (read non-inverted: ON = 1) ---- */

void dmx_address_init(void)
{
#define X(p,n)  REG(TRIS,p) |= MASK(n); REG(ANSEL,p) &= (unsigned char)~MASK(n);
    ADDR_PINS(X)
#undef X
}

unsigned int dmx_address_read(void)
{
    unsigned int a = 0;
    unsigned char bit = 0;
#define X(p,n)  if (REG(PORT,p) & MASK(n)) a |= (1u << bit); bit++;
    ADDR_PINS(X)
#undef X
    return (a > DMX_CHANNELS) ? DMX_CHANNELS : a;
}
