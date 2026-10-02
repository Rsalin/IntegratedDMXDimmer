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

volatile unsigned char TramaDMX[TOTALCHANNELS]={0}; //Rx buffer
volatile unsigned int address;                    //Address variable
volatile rx_valid_t rx_valid;

volatile unsigned char DatoRX;                           
volatile unsigned int DMX_Indice, DMX_RX_buffer_index;

void usart_timeout_timer_init(unsigned char low, unsigned char high)
{
    T0CON0 = 0;               //Timer off, postscaler 1:1
    T0CON0bits.T016BIT=1;     //16 bit timer
    T0CON1bits.T0CS=0b010;    //T0 source Fosc/4 (16 MHz)
    T0CON1bits.T0ASYNC=0;     //Synchronized to Fosc/4
    T0CON1bits.T0CKPS=0b1000; //:256 prescaler
    TMR0H=high;               //High first: it is latched when TMR0L is written
    TMR0L=low;
    IPR0bits.TMR0IP=0;        //Low priority
}

inline void usart_timeout_reset(unsigned char low, unsigned char high)
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
        PIR0bits.TMR0IF=0; //Clear flag
    }
}


void usart_config(void)
{
    /*RX pin: RC1 (USART_RX). Called before interrupts are enabled*/
    TRISCbits.TRISC1 = 1;   // Set RC1 as input
    ANSELCbits.ANSELC1 = 0; // Make RC1 a digital input

    PPSLOCK = 0x55;         // Unlock sequence
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 0;
    RX1PPS = 0x11;          // RC1 -> EUSART1 RX (PORTC = 0b10, pin 1)
    PPSLOCK = 0x55;         // Lock sequence
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1;

      /*USART configurations*/
    TX1STAbits.BRGH=1;          // Alta velocidad seleccionada.
    BAUD1CONbits.BRG16=1;       // Baudrate de 16 bits
    TX1STAbits.SYNC=0;          // Seleccionamos transmisión asíncrona
    
    SP1BRGL=63;                 // Fosc/(4*(63+1)) = 64MHz/256 = 250 kbaud
    SP1BRGH=0;
    RC1STAbits.RX9=1;           // Activada la recepción a 9 bits
    RC1STAbits.SREN=0;          // Desactivada la recepción de un sólo byte
    RC1STAbits.ADDEN=0;         // Desactivada la autodetección de dirección
    RC1STAbits.SPEN=1;          // USART activada
    RC1STAbits.CREN=1;          // Recepción activada
    //TX1STAbits.TXEN=0;          //TX off
    
    DMX_Estado = DMX_ESPERA_BREAK;

    /*Interrupt Configuration*/
    IPR3bits.RC1IP=0;            //Low priority for EUSART  
    PIE3bits.RC1IE = 1;          //EUSART1 Receiving Interrupt Enable
}

inline void usart_isr(void)
{    
    if (PIR3bits.RC1IF)
    {
        Copia_RCSTA.registro = RC1STA;  //FERR/RX9D of the byte on top of the FIFO: read before RC1REG
        DatoRX = RC1REG;
    
        if (Copia_RCSTA.bits.OERR)
        {
        RC1STAbits.CREN=0;
        RC1STAbits.CREN=1;
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

/*
 * DMX address DIP switch (9 bits)
 * addr0-1: RC6-RC7, addr2-5: RD4-RD7, addr6-8: RB0-RB2
 */
void address_init(void)
{
    TRISC|=0xC0;   //RC6-7 as inputs
    ANSELC&=0x3F;
    TRISD|=0xF0;   //RD4-7 as inputs
    ANSELD&=0x0F;
    TRISB|=0x07;   //RB0-2 as inputs
    ANSELB&=0xF8;
}

unsigned int read_address(void)
{
    unsigned int address;

    address=(unsigned int)((PORTC>>6)&0x03);          //addr0-1
    address|=(unsigned int)((PORTD>>4)&0x0F)<<2;      //addr2-5
    address|=(unsigned int)(PORTB&0x07)<<6;           //addr6-8
    return ((address+ADDRESS_OFFSET>TOTALCHANNELS)?TOTALCHANNELS:(address+ADDRESS_OFFSET));
}

/* TODO implementar más adelante
void __interrupt(low_priority) dmx_isr(void) {
    // Verificar si la interrupción fue por la UART1
    if (PIR3bits.RC1IF) {
        
        // 1. Manejo de Errores de Overrun (OERR)
        // Ocurre si no leemos a tiempo. Bloquea la recepción.
        if (RCSTA1bits.OERR) {
            RCSTA1bits.CREN = 0; // Resetear el módulo
            RCSTA1bits.CREN = 1;
            dmx_byte_count = 0;   // Reiniciar contador por seguridad
            return;
        }

        // 2. Detección de BREAK (Error de Frame + Dato 0)
        // El Break es un pulso bajo largo que genera un FERR.
        if (RCSTA1bits.FERR) {
            unsigned char dummy = RC1REG; // Leer para limpiar el error
            dmx_byte_count = 0;           // El siguiente byte será el Start Code
            dmx_ready = 0;                // Estamos empezando a recibir
            return;
        }

        // 3. Recepción de Datos
        unsigned char incoming_byte = RC1REG; // Leer el dato recibido

        if (dmx_byte_count == 0) {
            // El primer byte tras el Break es el START CODE
            // DMX estándar usa 0x00. Si es distinto, ignoramos el paquete.
            if (incoming_byte == 0x00) {
                dmx_byte_count = 1;
            } else {
                dmx_byte_count = 513; // Valor fuera de rango para ignorar trama
            }
        } else if (dmx_byte_count <= 512) {
            // Guardar el dato en tu array si coincide con tu dirección
            // Supongamos que tu dimmer empieza en la dirección 'dmx_address'
            if (dmx_byte_count >= dmx_address && dmx_byte_count < (dmx_address + NUM_CHANNELS)) {
                dmx_data[dmx_byte_count - dmx_address] = incoming_byte;
            }
            dmx_byte_count++;
        }
    }
}
 * */