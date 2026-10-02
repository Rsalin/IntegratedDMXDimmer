/*
 * File:   dimmer.c
 * Author: Josefe
 * 
 * Created on 17 April 2017, 10:46
 */

/*
 * Digital phase-controlled dimmer with mains frequency measurement. 
 * Behavioral description file. All functions needed ton control it are included here.
 * For using it, just initialize it, set ISR functions, enable them and pass dimming value.
 * It works with 256 discrete time slots, where, according to the AC RMS value required, the output will be fired if the AC phase is bigger than the required RMS phase value.
 * When the AC phase goes to 0, outputs are turned off, AC frequeny updated and reseted for a new firing cycle that will begin when Zero crossing ends.
 * In case of AC fail, firing goes off.
 */

#include <xc.h>
#include "build_config.h"
#include "dimmer_hal.h"
#include "dimmer.h"

#ifdef no_inline
    #define inline
#endif

//Dimmer states
typedef enum 
{
    DIMMER_IDLE,                    //Dimmer is waiting for first frequency measurement      
    DIMMER_START_FREQ_MEASURING,    //Dimmer is measuring frequency previously to start firing
    DIMMER_FREQ_LOADED,             //AC waveform goes to 0 and outputs are turned off
    DIMMER_ZC,                      //Same but after a firing process
    DIMMER_FIRING                   //Dimmer is firing channels and remeasuring frequency
}states;

//Linear firing rom (see dimmer.h)
const unsigned char firing_map[256] = {255, 238, 232, 227, 224, 221, 218, 216, 214, 212, 210, 208, 207, 205, 204, 202, 201, 200, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180, 180, 179, 178, 177, 176, 176, 175, 174, 173, 173, 172, 171, 170, 170, 169, 168, 168, 167, 166, 166, 165, 164, 164, 163, 162, 162, 161, 160, 160, 159, 158, 158, 157, 157, 156, 155, 155, 154, 154, 153, 152, 152, 151, 151, 150, 149, 149, 148, 148, 147, 147, 146, 145, 145, 144, 144, 143, 143, 142, 141, 141, 140, 140, 139, 139, 138, 138, 137, 136, 136, 135, 135, 134, 134, 133, 133, 132, 132, 131, 131, 130, 129, 129, 128, 128, 127, 127, 126, 126, 125, 125, 124, 124, 123, 123, 122, 121, 121, 120, 120, 119, 119, 118, 118, 117, 117, 116, 116, 115, 114, 114, 113, 113, 112, 112, 111, 111, 110, 110, 109, 108, 108, 107, 107, 106, 106, 105, 105, 104, 103, 103, 102, 102, 101, 101, 100, 99, 99, 98, 98, 97, 96, 96, 95, 95, 94, 93, 93, 92, 92, 91, 90, 90, 89, 89, 88, 87, 87, 86, 85, 85, 84, 83, 83, 82, 81, 81, 80, 79, 78, 78, 77, 76, 75, 75, 74, 73, 72, 72, 71, 70, 69, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 50, 49, 48, 46, 45, 44, 42, 40, 39, 37, 35, 33, 31, 28, 25, 22, 17, 11, 0};

unsigned char slot_counter;                          //AC firing slot counter
unsigned char fire_tresholds_buffer[NUM_CHANNELS];   //Channel's slots where it has to be fired
states dimmer_status;                       //State variable

/*
 * Updates channel's dimming values.
 * It receives the dimming values and its size
 * It sets the firing slots according to a linear firing rom (AC RMS value is LINEAR to dimming value)
 */
void set_fire_tresholds_buffer(unsigned char* data, unsigned char data_length)
{
    unsigned char i;
    for (i=0; i<data_length && i<NUM_CHANNELS; ++i )
    {
        if (data[i] == 0)
        {
            fire_tresholds_buffer[i]=NEVER_FIRE_SLOT; //Channel off: never fired
        }
        else
        {
            fire_tresholds_buffer[i]=(firing_map[data[i]]>>1); //128 dimming steps
        }
    }
}

/*
 * Provides a pointer to the shaped firing buffer
 */

unsigned char* get_fire_tresholds_buffer(void)
{
    return fire_tresholds_buffer;
}

/*
 * Initializes dimmer HW with data
 * It receives the first dimming values and its size
 * 
 */
void dimmer_init (unsigned char* data, unsigned char data_length)
{
    set_fire_tresholds_buffer(data, data_length); 
    slot_counter=RESET_SLOT;        //Reset values
    dimmer_status=DIMMER_IDLE;      
    channels_init();                //Port init
    zc_init();                      //Zero crossing init
    firing_timer_init();            //Firing slot timer init
    freq_measuring_timer_init();    

}

/*
 * Interrupt service routine for zero crossing process
 */
inline void zc_isr(void) 
{
    if (zc_check_flag())
    {      
        zc_clear_flag();
        switch (dimmer_status)
        {
            case DIMMER_START_FREQ_MEASURING: //First falling edge has happened
                //Turn all on could be used
                zc_set_edge_direction(ZC_rising_edge); //Waits for rising edge
                firing_timer_disable(); //No firing
                freq_measuring_timer_restart();   //Starts first frequency measurement             
                dimmer_status=DIMMER_FREQ_LOADED; //Next rising is a valid zero crossing
#ifdef debug
                debug_pin=0;
#endif
                break;
            case DIMMER_ZC:          //Valid zero crossing
            case DIMMER_FREQ_LOADED: //First measurement rising edge has happened
                turn_all_off(); //Turn all triac gates off
                zc_set_edge_direction(ZC_falling_edge);   //Waits for falling edge (and re-start firing) 
                firing_timer_disable(); //Stops firing timer
                firing_timer_update_period(freq_measuring_timer_freeze()); //Updates measured freq
                slot_counter=RESET_SLOT; //Resets slot counter too                
                dimmer_status=DIMMER_FIRING; //Next falling is firing!!
#ifdef debug
                debug_pin=1;
#endif                
                break;
            case DIMMER_FIRING: //Falling has happened, firing begins
                zc_set_edge_direction(ZC_rising_edge); //Waits for rising edge and zero crossing                  
                firing_timer_enable(); //Resets and starts firing timer
                //firing_timer_reset();  
                freq_measuring_timer_restart(); //Measure again
                dimmer_status=DIMMER_ZC; //waits for ZC
#ifdef debug
                debug_pin=0;
#endif                
                break;            
            default:
            case DIMMER_IDLE: //A rising edge has happened before first measuring
                turn_all_off(); //Output is off
                zc_set_edge_direction(ZC_falling_edge); //Wait for falling
                freq_measuring_timer_freeze(); //No measuring freq
                firing_timer_disable();        //No firing
                dimmer_status=DIMMER_START_FREQ_MEASURING; //Next falling starts measuring for the first time
#ifdef debug
                debug_pin=1;
#endif
                break;
        }      
    }
}

/*
 * ISR for firing channels
 */

inline void firing_timer_isr(void)
{
    if (firing_timer_check_flag())
    {
        firing_timer_clear_flag();
        if (slot_counter<LAST_SLOT) //If not all the time slots have passed
        {
            firing_timer_reset();  //Resets slot increase timer
            fire_all(slot_counter, fire_tresholds_buffer); //Check if any channel  has to be fired
            ++slot_counter; //Increses slot            
        } 
    }
}

/*
 * ISR for the frequency measuring timer overflow
 * If it happens, AC has gone off and firing must be stopped to prevent any damage
 */
inline void freq_measuring_timer_overflow_isr(void)
{
    if(freq_measuring_timer_check_flag())
    {
        freq_measuring_timer_clear_flag();
        turn_all_off(); //Output is turned off
        zc_set_edge_direction(ZC_rising_edge); //Wait for a rising edge
        freq_measuring_timer_freeze(); //No measuring freq
        firing_timer_disable();        //No firing
        //firing_timer_reset_period();   //Max period
        dimmer_status=DIMMER_IDLE; //waits for a new valid measurement
    }
}
