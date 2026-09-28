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
    IPR0bits.TMR0IP=0;     //Low priority
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
    TXSTAbits.SYNC=0;           // Seleccionamos transmisiï¿½n asï¿½ncrona
    
    SPBRG=0x3F;                 // A 64Mhz representa Baudios = 250KHz
    SPBRGH=0;
    RCSTAbits.RX9=1;            // Activada la recepciï¿½n a 9 bits
    RCSTAbits.SREN=0;           // Desactivada la recepciï¿½n de un sï¿½lo byte
    RCSTAbits.ADDEN=0;          // Desactivada la autodetecciï¿½n de direcciï¿½n
    RCSTAbits.FERR=0;           // No hay error de frame
    RCSTAbits.OERR=0;           // No hay error de overrun
    RCSTAbits.SPEN=1;           // USART activada
    RCSTAbits.CREN=1;           // Recepciï¿½n activada
    //TXSTAbits.TXEN=0;           //TX off
    
    //Unlock PPS
    GIE = 0; // Disable interrupts
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 0; // Unlock PPS

    TRISCbits.TRISC1 = 1;  // Set RC1 as input
    ANSELCbits.ANSELC1 = 0; //Make RC1 as digital input
    RX1PPS = 0x11; // RC1 as EUSART1 RX input

    //lock PPS
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1; // Lock PPS
    GIE = 1; // Re-enable interrupts

    /*Interrupt Configuration*/
    PIE3bits.RC1IE = 1;          //EUSART1 Receiving Interrupt Enable
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
        RCSTAbits.CREN=0;
        RCSTAbits.CREN=1;
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
}

unsigned int read_address(void)
{
    unsigned int address;

    address=(PORTC&0x07)|((PORTD&0x03)<<3)|(PORTC&0x20)|((PORTC&0x10)<<2)|((PORTD&0x08)<<4); //RD3-RC4-RC5-RD1-RD0-RC2-RC1-RC0    
    address+=(((PORTD&0x04)>>2)*256);//+256*PORTDbits.RD2;
    //return ((address+ADDRESS_OFFSET>TOTALCHANNELS)?TOTALCHANNELS:(address+ADDRESS_OFFSET));
    return 0;
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