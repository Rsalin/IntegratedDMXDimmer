/*
 * dimmer.c - Digital phase-control dimmer, fixed mains frequency (MAINS_HZ).
 *
 * ZC input: the falling edge starts the firing window, the rising edge is the
 * mains zero crossing (all outputs off). The window is split in SLOTS slots of
 * SLOT_TICKS (Timer3 + CCP1 compare, free running, no drift). On each slot,
 * every channel whose threshold has been reached is fired. If the rising edge
 * never comes (mains lost) everything is turned off one slot after the window.
 */
#include "dimmer.h"

#define NEVER_FIRE_SLOT 255     // Never reached: slots are < SLOTS <= 255

/* Linear firing table for 50 Hz: dimming value -> firing slot (0..255 scale) */
static const unsigned char firing_map[256] = {255, 238, 232, 227, 224, 221, 218, 216, 214, 212, 210, 208, 207, 205, 204, 202, 201, 200, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180, 180, 179, 178, 177, 176, 176, 175, 174, 173, 173, 172, 171, 170, 170, 169, 168, 168, 167, 166, 166, 165, 164, 164, 163, 162, 162, 161, 160, 160, 159, 158, 158, 157, 157, 156, 155, 155, 154, 154, 153, 152, 152, 151, 151, 150, 149, 149, 148, 148, 147, 147, 146, 145, 145, 144, 144, 143, 143, 142, 141, 141, 140, 140, 139, 139, 138, 138, 137, 136, 136, 135, 135, 134, 134, 133, 133, 132, 132, 131, 131, 130, 129, 129, 128, 128, 127, 127, 126, 126, 125, 125, 124, 124, 123, 123, 122, 121, 121, 120, 120, 119, 119, 118, 118, 117, 117, 116, 116, 115, 114, 114, 113, 113, 112, 112, 111, 111, 110, 110, 109, 108, 108, 107, 107, 106, 106, 105, 105, 104, 103, 103, 102, 102, 101, 101, 100, 99, 99, 98, 98, 97, 96, 96, 95, 95, 94, 93, 93, 92, 92, 91, 90, 90, 89, 89, 88, 87, 87, 86, 85, 85, 84, 83, 83, 82, 81, 81, 80, 79, 78, 78, 77, 76, 75, 75, 74, 73, 72, 72, 71, 70, 69, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 50, 49, 48, 46, 45, 44, 42, 40, 39, 37, 35, 33, 31, 28, 25, 22, 17, 11, 0};

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
        fire_thresholds[i] = data[i] ?
            (unsigned char)(((unsigned int)firing_map[data[i]] * SLOTS) >> 8) : NEVER_FIRE_SLOT;
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
    T3CLKbits.CS = 0b0001;
    T3CONbits.RD16 = 1;
    CCPTMRSbits.C1TSEL = 0b10;      // CCP1 uses Timer3
    CCP1CON = 0;
    CCP1CONbits.MODE = 0b1010;      // Compare, interrupt on match (no pin)
    CCP1CONbits.EN = 1;
    PIE6bits.CCP1IE = 0;
    PIR6bits.CCP1IF = 0;
    IPR6bits.CCP1IP = 1;

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
        if (slot_counter < SLOTS)
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
