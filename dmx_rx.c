/*
 * File:   dmx_rx.c
 * Author: josefe
 *
 * Created on 24 de junio de 2017, 18:01
 */


#include <xc.h>
#include "dmx_rx.h"

volatile union  // Estructura para hacer una copia del registro RCSTA
   {
   unsigned char registro;
   struct {
     unsigned char RX9D:1;
     unsigned char OERR:1;
     unsigned char FERR:1;
     unsigned char ADDEN:1;
     unsigned char CREN:1;
     unsigned char SREN:1;
     unsigned char RX9:1;
     unsigned char SPEN:1;
           } bits ;
  }Copia_RCSTA;
  
volatile enum 
{
    DMX_RECEPCION_DATOS,
    DMX_ESPERA_BYTE,
    DMX_ESPERA_BREAK,
    DMX_ESPERA_START
    
}DMX_Estado;

volatile unsigned char DatoRX;                           
volatile unsigned int DMX_Indice, DMX_RX_buffer_index;

void usart_timeout_timer_init(char low, char high)
{
    TMR0H=high;
    TMR0L=low;
    T0CON0bits.T016BIT=1;     //16 bit timer
    T0CON1bits.T0CS=2;       //T0 source Internal instruction cycle clock (CLKO) (Fosc/4)
    //RBS TODO: configure prescaler
    IPR0.TMR0IP=0;     //Low priority
}

inline void usart_timeout_reset(char low, char high)
{
    TMR0H=high;
    TMR0L=low;
    rx_valid=DATA_RX_VALID; //Validates data
    T0CON0bits.T0EN=1; //Turn on timer
    PIE0bits.TMR0IE=1; //Enable timer interrupt
    PIR0bits.TMR0IF=0; //Clear flag    
}

inline void usart_timeout_isr(void)
{
    if (PIE0bits.TMR0IE && PIR0bits.TMR0IF)
    {
        rx_valid=DATA_RX_INVALID; //Invalidates data
        T0CON0bits.T0EN=0; //Turn off timer
        PIE0bits.TMR0IE=0; //Disable timer interrupt
        
    }
}


void usart_config(void)
{
      /*USART configurations*/
    TXSTAbits.BRGH=1;           // Alta velocidad seleccionada.
    BAUDCONbits.BRG16=1;        // Baudrate de 16 bits
    TXSTAbits.SYNC=0;           // Seleccionamos transmisión asíncrona
    
    SPBRG=47;                    // A 48MHz representa Baudios = 250KHz
    SPBRGH=0;
    RCSTAbits.RX9=1;            // Activada la recepción a 9 bits
    RCSTAbits.SREN=0;           // Desactivada la recepción de un sólo byte
    RCSTAbits.ADDEN=0;          // Desactivada la autodetección de dirección
    RCSTAbits.FERR=0;           // No hay error de frame
    RCSTAbits.OERR=0;           // No hay error de overrun
    RCSTAbits.SPEN=1;           // USART activada
    RCSTAbits.CREN=1;           // Recepción activada
    //TXSTAbits.TXEN=0;           //TX off
    
    /*Interrupt Configuration*/
    PIE3bits.RC1IE = 1           //EUSART1 Receiving Interrupt Enable
    PIR3bits.RC1IF=0;            //Clear EUSART interruption flag
    IPR3bits.RC1IP=0;            //Low priority for EUSART  
}

inline void usart_isr(void)
{    
    if (PIR3bits.RC1IF)
    {
        Copia_RCSTA.registro = RCSTA;    
        DatoRX = RCREG;
    
        if (Copia_RCSTA.bits.OERR)
        {
        CREN=0;
        CREN=1;
        DMX_Estado = DMX_ESPERA_BYTE;
        return;
        }    
        
        switch (DMX_Estado)
        {
        case DMX_ESPERA_BYTE:
            if (!Copia_RCSTA.bits.FERR)
            {
              DMX_Estado = DMX_ESPERA_BREAK;
            }
            break;
            
        case DMX_ESPERA_BREAK:  
            if (Copia_RCSTA.bits.FERR)
            {
              if (!DatoRX)
              {
                DMX_Estado = DMX_ESPERA_START;
              }
            }
            break;
        
        case DMX_ESPERA_START: 
            if (Copia_RCSTA.bits.FERR)
            {
                DMX_Estado = DMX_ESPERA_BYTE;
            }
            else 
            {
                if (!DatoRX)
                {                   
                    DMX_Indice = 0;
                    DMX_RX_buffer_index=0;
                    DMX_Estado = DMX_RECEPCION_DATOS;
                } 
                else
                {
                    DMX_Estado = DMX_ESPERA_BREAK;
                }
            }
            break;
        case DMX_RECEPCION_DATOS:
            if (Copia_RCSTA.bits.FERR)
            {
              if (!DatoRX)
                {
                    DMX_Estado = DMX_ESPERA_START;
                }
                else
                {
                    DMX_Estado = DMX_ESPERA_BYTE;
                }
            }
            else
            {       
                
                if (DMX_Indice>=address)
                {
                    TramaDMX[DMX_RX_buffer_index++] = DatoRX;
                }
                DMX_Indice++;                  

                if (DMX_RX_buffer_index >= NUM_CHANNELS || DMX_Indice >= TOTALCHANNELS)
                {                    
                    DMX_Estado = DMX_ESPERA_BREAK;
                    TMR0H = USART_TIMEOUT_H;
                    TMR0L = USART_TIMEOUT_L;
                    rx_valid = DATA_RX_VALID; //Validates data
                    T0CON0bits.T0EN = 1; //Turn on timer
                    PIE0bits.TMR0IE = 1; //Enable timer interrupt
                    PIR0bits.TMR0IF = 0; //Clear flag                       
                }
            }
            break;
        }
    }
}

void address_init(void)
{
    TRISD|=0x0F; //RD3-0 as inputs
    TRISC|=0x07; //RC0-2 as inputs
    UCFG|=0x08;  //USB disabled
    UCON&=~(0x08);
    
}

unsigned int read_address(void)
{
    unsigned int address;
    address=(PORTC&0x07)|((PORTD&0x03)<<3)|(PORTC&0x20)|((PORTC&0x10)<<2)|((PORTD&0x08)<<4); //RD3-RC4-RC5-RD1-RD0-RC2-RC1-RC0    
    address+=(((PORTD&0x04)>>2)*256);//+256*PORTDbits.RD2;
    return ((address+ADDRESS_OFFSET>TOTALCHANNELS)?TOTALCHANNELS:(address+ADDRESS_OFFSET));
}