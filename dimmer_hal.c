/*
 * File:   dimmer_hal.c
 * Author: Josefe
 *
 * Created on 17 April 2017, 10:49
 */


#include "build_config.h"
#include <xc.h>
#include "dimmer_hal.h"

#ifdef no_inline
    #define inline
#endif

/*
 * Channel IO tables. Outputs are written through LATx: on the Q10 reading
 * PORTx returns the pin level (0 on analog pins), so read-modify-write on
 * PORTx could clear other channels of the same port.
 */
volatile unsigned char* const output_channels_latches[NUM_CHANNELS] = {&LATB, &LATB, &LATA, &LATA, &LATC, &LATC, &LATD, &LATC};
volatile unsigned char* const output_channels_tris[NUM_CHANNELS] = {&TRISB, &TRISB, &TRISA, &TRISA, &TRISC, &TRISC, &TRISD, &TRISC};
volatile unsigned char* const output_channels_ansel[NUM_CHANNELS] = {&ANSELB, &ANSELB, &ANSELA, &ANSELA, &ANSELC, &ANSELC, &ANSELD, &ANSELC};
const unsigned char output_channels_masks[NUM_CHANNELS] = {1<<3, 1<<4, 1<<2, 1<<3, 1<<5, 1<<4, 1<<3, 1<<3};

/*ZERO CROSSING FUNCTIONS*/

/*
 * Initializes Zero crossing interrupt-on-change pin (RE2)
 */
void zc_init(void)
{
    trisZC = 1;  //set as input pin
    anselZC = 0; //digital input buffer enabled
    ioc_negative_edgeZC = 0; //Sets first valid edge as a rising edge
    ioc_positive_edgeZC = 1;
    flagZC = 0;
    priorityZC = 1; //High priority
    interrupt_enableZC = ZC_enabled; //Enables interrupts
}

/*
 * Checks Zero crossing interrupt flag
 */
inline char zc_check_flag(void)
{
    return (flagZC && interrupt_enableZC);
}

/*
 * Clears Zero crossing interrupt flag
 */
inline void zc_clear_flag(void)
{
    flagZC=0;
}

/*
 * Changes Zero crossing edge direction and clears flag
 * Needs the desired new edge
 */
inline void zc_set_edge_direction(char direction)
{
    if (direction == ZC_rising_edge)
    {
        ioc_negative_edgeZC = 0;
        ioc_positive_edgeZC = 1;
    }
    else
    {
        ioc_positive_edgeZC = 0;
        ioc_negative_edgeZC = 1;
    }
    flagZC=0;
}

/*SLOT COUNTER TIMER FUNCTIONS*/

/*
 * Initializes slot counter timer to produce an interrupt each time the slot time has passed.
 * Uses CCP1 as output comparator and timer 3 as timebase.
 * Timer3 runs at Fosc/4 / 8 = 2 MHz, half the freq measuring timer clock (4 MHz),
 * so a period equal to TMR1H gives 128 slots per measured half cycle.
 */
void firing_timer_init(void)
{
    T3CON = 0;                //Timer off
    T3GCON = 0;               //No gate
    T3CLKbits.CS = 0b0001;    //Fosc/4 (16 MHz)
    T3CONbits.CKPS = 0b11;    //:8 prescaler
    T3CONbits.RD16 = 1;       //One single 16 bit write (TMR3H first)
    TMR3H = 0;                //Timer count reset
    TMR3L = 0;

    CCPTMRSbits.C1TSEL = 0b10; //CCP1 compare based on Timer3

    CCPR1H = 0xFF;            //Inits CCP on max compare
    CCPR1L = 0xFF;
    CCP1CON = 0;
    CCP1CONbits.MODE = 0b1010; //Compare mode: interrupt on match (CCP1 not routed to any pin)
    CCP1CONbits.EN = 1;

    PIE6bits.CCP1IE = 0;      //Enabled by firing_timer_enable()
    PIR6bits.CCP1IF = 0;      //Clear interrupt flag
    IPR6bits.CCP1IP = 1;      //High priority for ccp match
}

/*
 * Starts the slot timer counter
 */
inline void firing_timer_enable(void)
{
    TMR3H = 0;               //Count reset
    TMR3L = 0;
    PIE6bits.CCP1IE = 1;     //Interrupts on
    PIR6bits.CCP1IF = 0;     //Clear interrupt flag
    T3CONbits.ON = 1;        //Timer on
}

/*
 * Stops firing timer
 */
inline void firing_timer_disable(void)
{
    PIE6bits.CCP1IE = 0;     //Interrupts off
    T3CONbits.ON = 0;        //Timer off
}

/*
 * Resets firing timer
 */
inline void firing_timer_reset(void)
{
    TMR3H=0;          // Count reset
    TMR3L=0;
}

/*
 * Checks firing timer flag
 */
inline char firing_timer_check_flag(void)
{
    return (PIE6bits.CCP1IE && PIR6bits.CCP1IF);
}

/*
 * Clears firing timer flag
 */
inline void firing_timer_clear_flag(void)
{
    PIR6bits.CCP1IF = 0;
}

/*
 * Updates firing timer slot period
 * Each time the period has passed, one slot is incremented
 * Period is shifted straight away to the low register
 */
inline void firing_timer_update_period(unsigned char period)
{
    CCPR1H=0;       //First High, second low to allow a proper reset
    CCPR1L=period;
}

/*
 * Sets the firing timer slot period to its max value
 */
inline void firing_timer_reset_period(void)
{
    CCPR1H=0xFF; //Inits CCP on max compare
    CCPR1L=0xFF;
}

/*FREQUENCY MEASUREMENT TIMER FUNCTIONS*/

/*
 * Initializes the frequency measurement timer
 * the measured value will be used to calculate the comparison treshold to produce the slot increment.
 * This allows a dynamic frecuency measurement.
 * If it overflows means that zero crossing hasn't appeard,indicating that mains has gone away.
 * It is important to allow a full measurement range for the lowest frequency value without overflowing.
 * Timer1 runs at Fosc/4 / 4 = 4 MHz: overflows after 16.4 ms (10 ms half cycle @50Hz -> TMR1H ~156)
 */
inline void freq_measuring_timer_init(void)
{
    T1CON = 0;              //Timer off, RD16=0 so TMR1H can be read alone
    T1GCON = 0;             //No gate
    T1CLKbits.CS = 0b0001;  //Fosc/4 (reset value selects the T1CKI pin)
    T1CONbits.CKPS = 0b10;  //:4 prescaler
    PIE4bits.TMR1IE = 0;    //Enabled by freq_measuring_timer_restart()
    PIR4bits.TMR1IF = 0;
    IPR4bits.TMR1IP = 1;    //High priority
}

/*
 * Restarts frequency measuring timer
 */
inline void freq_measuring_timer_restart (void)
{
    PIE4bits.TMR1IE=1;  //Enable interrupts
    PIR4bits.TMR1IF=0;
    TMR1H=0;            //Clear count
    TMR1L=0;
    T1CONbits.ON=1;     //start timer
}

/*
 * Stops the frequency measurement timer, returning the measured value
 */
inline unsigned char freq_measuring_timer_freeze(void)
{
    PIE4bits.TMR1IE=0;  //Interrupts off
    T1CONbits.ON=0;     //Timer off
    return TMR1H;
}

/*
 * Checks measuring timer overflow flag
 */
inline char freq_measuring_timer_check_flag(void)
{
    return (PIE4bits.TMR1IE && PIR4bits.TMR1IF);
}

/*
 * Clears measuring timer overflow flag
 */
inline void freq_measuring_timer_clear_flag(void)
{
    PIR4bits.TMR1IF=0;
}


/*OUTPUT FIRNG PORTS (I/O PORTS) FUNCTIONS*/

/*
 * Initializes firing ports, setting up tris as output ports and clearing the output value
 * It uses a table where each firing port latch, tris and mask are arranged according to the output channel numbers
 */
void channels_init (void)
{
    unsigned char i;
    for (i=0; i<NUM_CHANNELS; ++i)
    {
        *(output_channels_latches[i])&=(unsigned char)~output_channels_masks[i];
        *(output_channels_ansel[i])&=(unsigned char)~output_channels_masks[i];
        *(output_channels_tris[i])&=(unsigned char)~output_channels_masks[i];
    }
}

/*
 * Turns all firing ports off
 */
inline void turn_all_off(void)
{
    unsigned char i;
    for (i=0; i<NUM_CHANNELS; ++i)
    {
        *(output_channels_latches[i])&=(unsigned char)~output_channels_masks[i];
    }
}

/*
 * Checks and fires all the ports if the current time slot is bigger than the setted one
 * It requires the current slot value and the firing tresholds table pointer
 */
inline void fire_all(unsigned char count, unsigned char* fire_tresholds)
{
    unsigned char i;
    for (i=0; i<NUM_CHANNELS; ++i)
    {
        if (count>=fire_tresholds[i])
        {
            *(output_channels_latches[i])|=output_channels_masks[i];
        }
    }
}
