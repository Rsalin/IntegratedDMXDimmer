/*
 * dimmer.c - Digital phase-control dimmer.
 *
 * Zero crossing (ZC) input: falling edge starts the firing window, rising edge is
 * the mains zero crossing. Timer1 measures the time between both edges (TMR1H).
 * That window is split in 128 slots using Timer3 + CCP1 (compare, period = TMR1H).
 * On each slot, every channel whose threshold has been reached is fired.
 * On the ZC rising edge all outputs are turned off.
 * If Timer1 overflows (mains lost) everything is turned off and the state goes IDLE.
 */
#include "dimmer.h"

#define LAST_SLOT       255
#define NEVER_FIRE_SLOT 255     // slot_counter stops before this value

typedef enum
{
    DIMMER_IDLE,                // Waiting for a first edge
    DIMMER_START_FREQ_MEASURING,// Waiting for the falling edge to start measuring
    DIMMER_FREQ_LOADED,         // Measuring the first window
    DIMMER_ZC,                  // Firing, waiting for the zero crossing
    DIMMER_FIRING               // Zero crossed, waiting for the falling edge
} dimmer_state_t;

/* Linear firing table: dimming value -> firing slot (halved: 128 steps) */
static const unsigned char firing_map[256] = {255, 238, 232, 227, 224, 221, 218, 216, 214, 212, 210, 208, 207, 205, 204, 202, 201, 200, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180, 180, 179, 178, 177, 176, 176, 175, 174, 173, 173, 172, 171, 170, 170, 169, 168, 168, 167, 166, 166, 165, 164, 164, 163, 162, 162, 161, 160, 160, 159, 158, 158, 157, 157, 156, 155, 155, 154, 154, 153, 152, 152, 151, 151, 150, 149, 149, 148, 148, 147, 147, 146, 145, 145, 144, 144, 143, 143, 142, 141, 141, 140, 140, 139, 139, 138, 138, 137, 136, 136, 135, 135, 134, 134, 133, 133, 132, 132, 131, 131, 130, 129, 129, 128, 128, 127, 127, 126, 126, 125, 125, 124, 124, 123, 123, 122, 121, 121, 120, 120, 119, 119, 118, 118, 117, 117, 116, 116, 115, 114, 114, 113, 113, 112, 112, 111, 111, 110, 110, 109, 108, 108, 107, 107, 106, 106, 105, 105, 104, 103, 103, 102, 102, 101, 101, 100, 99, 99, 98, 98, 97, 96, 96, 95, 95, 94, 93, 93, 92, 92, 91, 90, 90, 89, 89, 88, 87, 87, 86, 85, 85, 84, 83, 83, 82, 81, 81, 80, 79, 78, 78, 77, 76, 75, 75, 74, 73, 72, 72, 71, 70, 69, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 50, 49, 48, 46, 45, 44, 42, 40, 39, 37, 35, 33, 31, 28, 25, 22, 17, 11, 0};

static unsigned char slot_counter;
static unsigned char fire_thresholds[NUM_CHANNELS];
static dimmer_state_t dimmer_state;

/* ---- ZC pin: interrupt-on-change ---- */
#define ZC_TRIS     REG(TRIS,ZC_PORT)
#define ZC_ANSEL    REG(ANSEL,ZC_PORT)
#define ZC_IOC_POS  REG3(IOC,ZC_PORT,P)
#define ZC_IOC_NEG  REG3(IOC,ZC_PORT,N)
#define ZC_IOC_FLAG REG3(IOC,ZC_PORT,F)
#define ZC_MASK     MASK(ZC_PIN)

#define ZC_RISING   1
#define ZC_FALLING  0

static void zc_set_edge(char rising)
{
    if (rising)
    {
        ZC_IOC_NEG &= (unsigned char)~ZC_MASK;
        ZC_IOC_POS |= ZC_MASK;
    }
    else
    {
        ZC_IOC_POS &= (unsigned char)~ZC_MASK;
        ZC_IOC_NEG |= ZC_MASK;
    }
    ZC_IOC_FLAG &= (unsigned char)~ZC_MASK;
}

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
static void firing_timer_enable(void)
{
    TMR3H = 0;
    TMR3L = 0;
    PIE6bits.CCP1IE = 1;
    PIR6bits.CCP1IF = 0;
    T3CONbits.ON = 1;
}

static void firing_timer_disable(void)
{
    PIE6bits.CCP1IE = 0;
    T3CONbits.ON = 0;
}

/* ---- Window timer (Timer1) ---- */
static void window_timer_restart(void)
{
    PIE4bits.TMR1IE = 1;
    PIR4bits.TMR1IF = 0;
    TMR1H = 0;
    TMR1L = 0;
    T1CONbits.ON = 1;
}

static unsigned char window_timer_freeze(void)
{
    PIE4bits.TMR1IE = 0;
    T1CONbits.ON = 0;
    return TMR1H;
}

/* ---- Public ---- */
void dimmer_set_levels(const unsigned char *data, unsigned char length)
{
    unsigned char i;
    for (i = 0; i < length && i < NUM_CHANNELS; ++i)
        fire_thresholds[i] = data[i] ? (firing_map[data[i]] >> 1) : NEVER_FIRE_SLOT;
}

void dimmer_init(const unsigned char *data, unsigned char length)
{
    dimmer_set_levels(data, length);
    slot_counter = 0;
    dimmer_state = DIMMER_IDLE;

    /* Outputs: low, digital, outputs */
#define X(p,n)  REG(LAT,p) &= (unsigned char)~MASK(n); \
                REG(ANSEL,p) &= (unsigned char)~MASK(n); \
                REG(TRIS,p) &= (unsigned char)~MASK(n);
    OUTPUT_PINS(X)
#undef X

    /* ZC input, first valid edge is a rising edge, high priority */
    ZC_TRIS |= ZC_MASK;
    ZC_ANSEL &= (unsigned char)~ZC_MASK;
    zc_set_edge(ZC_RISING);
    IPR0bits.IOCIP = 1;
    PIE0bits.IOCIE = 1;

    /* Timer3 (Fosc/4) + CCP1 compare on Timer3. Enabled by firing_timer_enable() */
    T3CON = 0;
    T3GCON = 0;
    T3CLKbits.CS = 0b0001;
    T3CONbits.CKPS = T3_PRESCALER;
    T3CONbits.RD16 = 1;
    TMR3H = 0;
    TMR3L = 0;
    CCPTMRSbits.C1TSEL = 0b10;      // CCP1 uses Timer3
    CCPR1H = 0xFF;
    CCPR1L = 0xFF;
    CCP1CON = 0;
    CCP1CONbits.MODE = 0b1010;      // Compare, interrupt on match (no pin)
    CCP1CONbits.EN = 1;
    PIE6bits.CCP1IE = 0;
    PIR6bits.CCP1IF = 0;
    IPR6bits.CCP1IP = 1;

    /* Timer1 (Fosc/4): half cycle measurement. RD16 = 0 so TMR1H reads alone */
    T1CON = 0;
    T1GCON = 0;
    T1CLKbits.CS = 0b0001;
    T1CONbits.CKPS = T1_PRESCALER;
    PIE4bits.TMR1IE = 0;
    PIR4bits.TMR1IF = 0;
    IPR4bits.TMR1IP = 1;
}

static void zc_isr(void)
{
    ZC_IOC_FLAG &= (unsigned char)~ZC_MASK;
    switch (dimmer_state)
    {
    case DIMMER_START_FREQ_MEASURING:   // First falling edge
        zc_set_edge(ZC_RISING);
        firing_timer_disable();
        window_timer_restart();
        dimmer_state = DIMMER_FREQ_LOADED;
        break;

    case DIMMER_ZC:                     // Valid zero crossing
    case DIMMER_FREQ_LOADED:
        turn_all_off();
        zc_set_edge(ZC_FALLING);
        firing_timer_disable();
        CCPR1H = 0;                     // High first, then low
        CCPR1L = window_timer_freeze();
        slot_counter = 0;
        dimmer_state = DIMMER_FIRING;
        break;

    case DIMMER_FIRING:                 // Falling edge: firing begins
        zc_set_edge(ZC_RISING);
        firing_timer_enable();
        window_timer_restart();
        dimmer_state = DIMMER_ZC;
        break;

    case DIMMER_IDLE:
    default:                            // Rising edge before the first measurement
        turn_all_off();
        zc_set_edge(ZC_FALLING);
        window_timer_freeze();
        firing_timer_disable();
        dimmer_state = DIMMER_START_FREQ_MEASURING;
        break;
    }
}

void dimmer_isr(void)
{
    if (PIE0bits.IOCIE && ZC_IOC_FLAG & ZC_MASK)
        zc_isr();

    /* Slot timer match */
    if (PIE6bits.CCP1IE && PIR6bits.CCP1IF)
    {
        PIR6bits.CCP1IF = 0;
        if (slot_counter < LAST_SLOT)
        {
            TMR3H = 0;
            TMR3L = 0;
            fire_all(slot_counter);
            ++slot_counter;
        }
    }

    /* Timer1 overflow: mains lost, stop firing */
    if (PIE4bits.TMR1IE && PIR4bits.TMR1IF)
    {
        PIR4bits.TMR1IF = 0;
        turn_all_off();
        zc_set_edge(ZC_RISING);
        window_timer_freeze();
        firing_timer_disable();
        dimmer_state = DIMMER_IDLE;
    }
}
