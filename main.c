/*
 * File:   main.c
 * Author: josefe
 *
 * Created on 21 de abril de 2017, 15:21
 */



#include "build_config.h"
#include <xc.h>
#define _XTAL_FREQ 32000000

#include "config.h"
#include "dimmer.h"
#include "dimmer_hal.h"
#include "dmx_rx.h"
#include "pic18f46q10.h"



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
    // 1. Set up OSCCON1 for External Osc with 4x PLL
    // NOSC = 0b010 (External Osc), NDIV = 0b0000 (Divide by 1)
    OSCCON1 = 0x20;

    // 2. Explicitly power up the External Oscillator circuit
    OSCENbits.EXTOEN = 1;

    // 3. Wait for Hardware Stability
    // Wait for the Crystal to physically start vibrating
    while(!OSCSTATbits.EXTOR); 
    
    // Wait for the PLL to lock onto the 8MHz and multiply it
    while(!OSCSTATbits.PLLR); 
    
#ifdef debug
    TRISDbits.TRISD0=0;
    PORTDbits.RD0=0;
#endif
    
    is_test_mode = check_test_mode();
    dimmer_init (channels_data, NUM_CHANNELS);
    usart_config();
    usart_timeout_timer_init(USART_TIMEOUT_L, USART_TIMEOUT_H);
    usart_timeout_reset(USART_TIMEOUT_L, USART_TIMEOUT_H);
    address_init();
    address = 0;
    if (is_test_mode == 1)
    {
        test_init();
    }
    
    //Enable interrupts  
    // Enable Interrupt Priority Vectors (16CXXX Compatibility Mode)
    INTCONbits.IPEN = 1;

    // Assign peripheral interrupt priority vectors

    // INT0I - high priority
    IPR0bits.INT0IP = 1;
    
    //Enables all low-priority interrupts
    INTCONbits.GIEL=1;
    //Enables all high-priority interrupts
    INTCONbits.GIEH=1;
    //Go!
    while(1)
    {
#ifdef debug
        /*
        TRISBbits.TRISB0=1;
        debug_pin=LATBbits.LATB0; //Attach interrupt line to output
         */
        
        //PORTDbits.RD0=0;
        //__delay_ms(5);
        PORTDbits.RD0=1;
        //__delay_ms(5);
         
#endif
        //TODO
        address=read_address();
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

unsigned char check_test_mode(void)
{
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