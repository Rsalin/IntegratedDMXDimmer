/*
 * File:   main.c
 * Author: josefe
 *
 * Created on 21 de abril de 2017, 15:21
 */



#include "build_config.h"
#include <xc.h>

#include "config.h"
#include "dimmer.h"
#include "dimmer_hal.h"
#include "dmx_rx.h"



void clock_init(void);
void osc_test(void);
unsigned char check_test_mode(void);
void process_channels(void);
void test_init(void);
void test_update(void);
void test(void);

//8 channels buffer
unsigned char channels_data[NUM_CHANNELS]={0};
unsigned char adc_buffer[NUM_CHANNELS]={0};

// User variables
unsigned char is_test_mode;
unsigned char channel_counter;
unsigned char global_fader;
enum directions
{
    INCREASING,
    DECREASING,
    NEXT_CHANNEL
}global_fader_direction;

void main(void) {
    unsigned int new_address;

    clock_init();

#ifdef debug
    ANSELDbits.ANSELD0=0;
    TRISDbits.TRISD0=0;
    LATDbits.LATD0=0;
#endif

    is_test_mode = check_test_mode();
    dimmer_init (channels_data, NUM_CHANNELS);
    usart_config();
    usart_timeout_timer_init(USART_TIMEOUT_L, USART_TIMEOUT_H);
    usart_timeout_reset(USART_TIMEOUT_L, USART_TIMEOUT_H);
    address_init();
    address = read_address();
    if (is_test_mode == 1)
    {
        test_init();
    }

    //Enable interrupts
    INTCONbits.IPEN=1;  //High/low priority vectors
    INTCONbits.GIEL=1;  //Enables all low-priority interrupts
    INTCONbits.GIEH=1;  //Enables all high-priority interrupts
    //Go!
    while(1)
    {
#ifdef debug
        /*
        TRISBbits.TRISB0=1;
        debug_pin=LATBbits.LATB0; //Attach interrupt line to output
         */

        //LATDbits.LATD0=0;
        //__delay_ms(5);
        LATDbits.LATD0=1;
        //__delay_ms(5);

#endif
        new_address=read_address();
        if (new_address != address)
        {
            INTCONbits.GIEL=0; //address is 16 bit and used by the DMX ISR
            address=new_address;
            INTCONbits.GIEL=1;
        }
        if (is_test_mode == 1)
        {
            test();
            test_update();
        }
        else
        {
            process_channels();
        }
        set_fire_tresholds_buffer(channels_data, NUM_CHANNELS);
    }
}
     
void __interrupt(high_priority) isr_high(void)
{
    zc_isr();
    firing_timer_isr();
    freq_measuring_timer_overflow_isr();
}

void __interrupt(low_priority)   isr_low(void)
{   
    usart_isr();
    usart_timeout_isr();  
}

/*
 * Oscillator: 16 MHz crystal (HS) x 4 PLL = 64 MHz.
 * RSTOSC already starts with EXTOSC_4PLL; the switch is requested again and
 * the code waits until the new oscillator and the PLL are ready.
 */
void clock_init(void)
{
    OSCCON1 = (0b010 << _OSCCON1_NOSC_POSN)  // NOSC EXTOSC with 4x PLL
            | (0b0000 << _OSCCON1_NDIV_POSN); // NDIV 1:1
    while(!OSCCON3bits.ORDY);   // Clock switch done
    while(!OSCSTATbits.EXTOR);  // Crystal running
    while(!OSCSTATbits.PLLR);   // PLL locked
}

unsigned char check_test_mode(void)
{
#if TEST_MODE_JUMPER == 0
    return 0; // No test mode jumper on this board
#else
    ANSELBbits.ANSELB5 = 0;
    ANSELBbits.ANSELB6 = 0;
    TRISBbits.TRISB6 = 1; // Input
    TRISBbits.TRISB5 = 0; // Output
    LATBbits.LATB5   = 0; // Low    
    __delay_ms(5);
    if (PORTBbits.RB6 != 0) // If no low return false
    {
        LATBbits.LATB5 = 0;
        TRISBbits.TRISB5 = 1;
        TRISBbits.TRISB6 = 1;
        return 0;
    }
    LATBbits.LATB5 = 1; // High
    __delay_ms(5);
    if (PORTBbits.RB6 != 1) // If no high return false
    {
        LATBbits.LATB5 = 0;
        TRISBbits.TRISB5 = 1;
        TRISBbits.TRISB6 = 1;
        return 0;
    }
    LATBbits.LATB5   = 0; // Low
    __delay_ms(5);
    if (PORTBbits.RB6 != 0) // If no low return false
    {
        LATBbits.LATB5 = 0;
        TRISBbits.TRISB5 = 1;
        TRISBbits.TRISB6 = 1;
        return 0;
    }
    LATBbits.LATB5 = 1; // High
    __delay_ms(5);
    if (PORTBbits.RB6 != 1) // If no high return false
    {
        LATBbits.LATB5 = 0;
        TRISBbits.TRISB5 = 1;
        TRISBbits.TRISB6 = 1;
        return 0;
    }
    // Sequence OK, set test mode
    LATBbits.LATB5 = 0;
    TRISBbits.TRISB5 = 1;
    TRISBbits.TRISB6 = 1;
    return 1;
#endif
}

void process_channels(void)
{
	unsigned char i;
	
	for (i=0; i<NUM_CHANNELS; i++)	// For each channel					
	{
		if (rx_valid==DATA_RX_VALID)// && address<(TOTALCHANNELS-i)) // If data or address aren't valid
            {
            if (address<(TOTALCHANNELS-i))
            
                {
                    if (TramaDMX[i]>adc_buffer[i])
                    {
                        channels_data[i]=TramaDMX[i];			// Passes value to output straight away
                    }
                    else
                    {
                        channels_data[i]=adc_buffer[i]; // Output is set to 0
                        //channels_data[i]=0;
                    } 
                }
		}
		else
		{
			channels_data[i]=adc_buffer[i]; // Output is set to 0
            //channels_data[i]=0;
		}   
	}
}

void test_init(void)
{
    global_fader = 0;
    channel_counter = 0;
    global_fader_direction = INCREASING;
}

void test_update(void)
{
    switch (global_fader_direction)
    {
        case INCREASING:
        {
            if (global_fader == 255)
            {
                --global_fader;
                global_fader_direction = DECREASING;
            }
            else
            {
                ++global_fader;
            }
            break;
        }
        case DECREASING:
        {
            if (global_fader == 0)
            {
                global_fader_direction = NEXT_CHANNEL;
            }
            else
            {
                --global_fader;
            }
            break;
        }
        case NEXT_CHANNEL:
        {
            global_fader = 0;
        }
        default:
        {
            break;
        }
    }
}

void test(void)
{
    unsigned char i;
    if (address == 256) // Only 9th bit
    {
        for (i = 0; i < NUM_CHANNELS; ++i)
        {
            if (i != channel_counter)
            {
                channels_data[i] = 0;
            }
        }
        channels_data[channel_counter] = global_fader;
        if (global_fader_direction == NEXT_CHANNEL)
        {
            ++channel_counter;
            global_fader_direction = INCREASING;
            if (channel_counter >= NUM_CHANNELS)
            {
                channel_counter = 0;
            }
        }        
        return;
    }
    if ((address > 256) && (address <= 511)) // 9th bit on and any between 1st to 8th
    {
        // Fading up and down all together if dip sw is on
        for (i = 0; i < NUM_CHANNELS; ++i)
        {
            if (address & (1 << i))
            {
                channels_data[i] = global_fader;
            }
            else
            {
                channels_data[i] = 0;
            }
        } 
        if (global_fader_direction == NEXT_CHANNEL)
        {
            global_fader_direction = INCREASING;
        }
        return;
    }
    if (address == 0) // No switches on
    {
        // AnalogMap
        for (i=0; i<NUM_CHANNELS; i++)	// For each channel
        {
            channels_data[i]=adc_buffer[i];
        }
        return;
    }
    if ((address > 0) && (address < 256))
    {
        // Only those channels full on
        for (i = 0; i < NUM_CHANNELS; ++i)
        {
            if (address & (1 << i))
            {
                channels_data[i] = 255;
            }
            else
            {
                channels_data[i] = 0;
            }
        }        
        return;
        
    }
    
}

void osc_test(void) {
    //Output Clock to Pin (RB3) for verification
    TRISBbits.TRISB3 = 0;
    ANSELBbits.ANSELB3 = 0;
    LATBbits.LATB3 = 0; // Turn off RB3 initially

    while(1) {
        LATBbits.LATB3 = 1;
        LATBbits.LATB3 = 0;
    }    
}