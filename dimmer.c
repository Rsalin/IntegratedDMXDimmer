/*
 * dimmer.c - Digital phase-control dimmer, fixed mains frequency (MAINS_HZ).
 *
 * ZC input: the falling edge starts the firing window, the rising edge is the
 * mains zero crossing (all outputs off). The window is split in SLOTS slots of
 * SLOT_TICKS (Timer3 + CCP1 compare, free running, no drift). On each slot,
 * every channel whose threshold has been reached is fired (256 slots = one per
 * DMX step). If the rising edge
 * never comes (mains lost) everything is turned off one slot after the window.
 */
#include "dimmer.h"
#include "firing_table.h"

/* PIC18-Q register values (datasheet DS40001996D) */
#define TxCLK_FOSC_4        0b00001     // Timer1/3 clock source: Fosc/4
#define CCP1_TIMER3         0b10        // CCPTMRS.C1TSEL: Timer3 in capture/compare mode
#define CCP_MODE_COMPARE    0b1010      // Compare, set CCPxIF on match, no output pin

#define NEVER_FIRE_SLOT 255     // Never reached: only slots 0..SLOTS-2 (254) fire; slot 255 ends the window

static unsigned char slot_counter;
static unsigned int next_match;
static unsigned char fire_thresholds[NUM_CHANNELS];

/* ---- ZC pin: interrupt-on-change, both edges ---- */
#define ZC_TRIS     REG(TRIS,ZC_PORT)
#define ZC_ANSEL    REG(ANSEL,ZC_PORT)
#define ZC_PORTREG  REG(PORT,ZC_PORT)
#define ZC_IOC_POS  REG3(IOC,ZC_PORT,P)
#define ZC_IOC_NEG  REG3(IOC,ZC_PORT,N)
#define ZC_IOC_FLAG REG3(IOC,ZC_PORT,F)
#define ZC_MASK     MASK(ZC_PIN)

/* ---- Output channels ---- */
static void turn_all_off(void)
{
#define X(p,n)  REG(LAT,p) &= (unsigned char)~MASK(n);
    OUTPUT_PINS(X)
#undef X
}

static void fire_all(unsigned char slot)
{
    unsigned char i = 0;
#define X(p,n)  if (slot >= fire_thresholds[i]) REG(LAT,p) |= MASK(n); i++;
    OUTPUT_PINS(X)
#undef X
}

/* ---- Slot timer (Timer3 + CCP1) ---- */
static void set_next_match(unsigned int t)
{
    next_match = t;
    CCPR1H = (unsigned char)(t >> 8);   // High first
    CCPR1L = (unsigned char)t;
}

static void firing_timer_start(void)
{
    T3CONbits.ON = 0;
    TMR3H = 0;
    TMR3L = 0;
    set_next_match(SLOT_TICKS);
    slot_counter = 0;
    PIR6bits.CCP1IF = 0;
    PIE6bits.CCP1IE = 1;
    T3CONbits.ON = 1;
}

static void firing_timer_stop(void)
{
    PIE6bits.CCP1IE = 0;
    T3CONbits.ON = 0;
}

/* ---- Public ---- */
void dimmer_set_levels(const unsigned char *data, unsigned char length)
{
    unsigned char i;
    for (i = 0; i < length && i < NUM_CHANNELS; ++i)
        fire_thresholds[i] = data[i] ? firing_map[data[i]] : NEVER_FIRE_SLOT;  // 1 slot per DMX step
}

void dimmer_init(const unsigned char *data, unsigned char length)
{
    dimmer_set_levels(data, length);

    /* Outputs: low, digital, outputs */
#define X(p,n)  REG(LAT,p) &= (unsigned char)~MASK(n); \
                REG(ANSEL,p) &= (unsigned char)~MASK(n); \
                REG(TRIS,p) &= (unsigned char)~MASK(n);
    OUTPUT_PINS(X)
#undef X

    /* Timer3 (Fosc/4, 1:1) + CCP1 compare on Timer3 */
    T3CON = 0;
    T3GCON = 0;
    T3CLKbits.CS = TxCLK_FOSC_4;
    T3CONbits.RD16 = 1;
    CCPTMRSbits.C1TSEL = CCP1_TIMER3;      // CCP1 uses Timer3
    CCP1CON = 0;
    CCP1CONbits.MODE = CCP_MODE_COMPARE;      // Compare, interrupt on match (no pin)
    CCP1CONbits.EN = 1;
    PIE6bits.CCP1IE = 0;
    PIR6bits.CCP1IF = 0;
    IPR6bits.CCP1IP = 1;

    /* ZC also wired to RE2: input + analog = high impedance, no digital buffer */
    REG(TRIS,ZC_ALT_PORT) |= MASK(ZC_ALT_PIN);
    REG(ANSEL,ZC_ALT_PORT) |= MASK(ZC_ALT_PIN);

    /* ZC input, both edges, high priority */
    ZC_TRIS |= ZC_MASK;
    ZC_ANSEL &= (unsigned char)~ZC_MASK;
    ZC_IOC_POS |= ZC_MASK;
    ZC_IOC_NEG |= ZC_MASK;
    ZC_IOC_FLAG &= (unsigned char)~ZC_MASK;
    IPR0bits.IOCIP = 1;
    PIE0bits.IOCIE = 1;
}

void dimmer_isr(void)
{
    if (PIE0bits.IOCIE && (ZC_IOC_FLAG & ZC_MASK))
    {
        ZC_IOC_FLAG &= (unsigned char)~ZC_MASK;
        turn_all_off();
        if (ZC_PORTREG & ZC_MASK)
            firing_timer_stop();        // Rising: zero crossing
        else
            firing_timer_start();       // Falling: firing window begins
    }

    if (PIE6bits.CCP1IE && PIR6bits.CCP1IF)
    {
        PIR6bits.CCP1IF = 0;
        if (slot_counter < SLOTS - 1)   // 8 bit counter: the last slot (255) is the window end
        {
            set_next_match(next_match + SLOT_TICKS);
            fire_all(slot_counter);
            ++slot_counter;
        }
        else
        {
            turn_all_off();             // Window over without zero crossing
            firing_timer_stop();
        }
    }
}
