// PIC16F886 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config DEBUG = OFF //DEBUG = ON
#pragma config FOSC = INTRC_NOCLKOUT //INTRC_CLKOUT --> // Oscillator Selection bits (INTOSC oscillator: CLKOUT function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF        // Watchdog Timer Enable bit (WDT enabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RE3/MCLR pin function select bit (RE3/MCLR pin function is MCLR)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = OFF       // Brown Out Reset Selection bits (BOR enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal/External Switchover mode is enabled)
#pragma config FCMEN = OFF       // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is enabled)
#pragma config LVP = OFF //do OFF          //Low Voltage Programming Enable bit (RB3/PGM pin has NO! - PGM function, low voltage programming enabled)
               //If LVP ON, it doesn't allow switch the modes program/debugger //30.11.2019
               //After debugging and programing hold reset the MCU, then MCLR connect to boards GND and disconnect the PICKIT-2;3(all pins) from your experimenting board.  
               //On CONFIG2 *DEBUG bit should be set to 1. Also you may pull-down the PGC,PGD(RB6;7) pins by R100k, that would not affect(ed? or hadn't) to debugging process and keeps safely from "air" voltages
// CONFIG2
#pragma config BOR4V = BOR21V   // Brown-out Reset Selection bit (Brown-out Reset set to 2.1V)
#pragma config WRT = OFF        // Flash Program Memory Self Write Enable bits (Write protection off)

#define _XTAL_FREQ 4000000
// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdint.h>

#define DISABLE_LOGS

#include "SimpleTimerWP.h"
#include "usart.h"
#include "HardwareInterfaceUnit.h"

extern uint32_t someExternalTick;
static uint32_t getTickValue(void);
InterfacePortHandle_t SlavePort;
Timert_t Timer1s;
Timert_t Timer10ms;
uint8_t txBuffer;
uint8_t rxBuffer;

static uint32_t getTickValue()
{
    return someExternalTick;
}

void __interrupt() ISR(void)
{
    if (TMR2IF == 1) // Check The Flag
    {
        // Do Timer handling
        someExternalTick++;
        TMR2IF = 0;   // Clear The Flag Bit !
        if ((SlavePort.Status & (PORT_READY | PORT_SENDING /*| PORT_BUSY*/)) == ONLY (PORT_READY | PORT_SENDING /*| PORT_BUSY*/)) {
            SlavePort.Status |= PORT_SENDED;
            Write(&SlavePort, NULL, no_required_now);
        }
	}
    if (ADIF == 1)  // Check The Flag
    {
        // Do ADC handling
        ADIF = 0;    // Clear The Flag Bit !
    }
    /*    if (RCIF == 1)
        {
            //USART RX interrupt handle
            RCIF = 0;
        }
        if (TXIF == 1)
        {
            //USART TX interrupt handle
            TXIF = 0;
        }
    */
    //if (PIR1bits.TXIF/*is almost 1 any time //!?*/ && PIE1bits.TXIE)  // Interrupt for data transmission
    //{
        //TransmitInterrupt(&SlavePort);
    //}
    if(RCSTAbits.FERR){
        //rx framing error
        SPEN = 0;
        SPEN = 1;
    }
    if(RCSTAbits.OERR){
        //rx overrun error!
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
    }
    
    if (PIR1bits.RCIF)  // Interrupt for data reception
    {
        ReceiveInterrupt(&SlavePort);
    }
}

void Timer2_to_1ms_interrupt_init()
{
    /* Fcycle = 4MHz/4 = 1Mhz */
    /* Fcycle/(Tckps[2LSB:0b01]) = 1MHz/4 = 250kHz */
    /* 250kHz --> |every 250 TMR2 cnt| = 1000Hz */
    T2CON = 0b00000000 | 0x01;
    PR2 = 250;
    TMR2ON = 0;
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;
    TMR2ON = 1;
}

void GlobalMCUINT_init()
{
    INTCONbits.PEIE = 1; // Enable peripheral interrupts
    INTCONbits.GIE = 1;  // Global interrupt enable
}

void main(void* arg)
{        
    Timer2_to_1ms_interrupt_init();
    InitTimerWP(&Timer1s, NULL);
    LaunchTimerWP(1000, &Timer1s);
    InitTimerWP(&Timer10ms, NULL);
    LaunchTimerWP((U32_ms)10, &Timer10ms);
    char buffer[RECV_BUFFER_SIZE];
    //char storedBuffer[RECV_BUFFER_SIZE];
    UART_Init();
    InitSlavePort(&SlavePort);
    SlavePort.Status setBITS(PORT_READY);
    GlobalMCUINT_init();
    while(NOT IsTimerWPRinging(&Timer1s));
    RestartTimerWP(&Timer1s);
    while (1)
    {
        // Sending data
        //UART_SendChar('A');

        // Delay
        //__delay_ms(1000);

        if (IsTimerWPRinging(&Timer10ms)) {
            RestartTimerWP(&Timer10ms);
            if ((SlavePort.Status & (PORT_BUSY | PORT_SENDING)) == NOTHING) {
                memset(SlavePort.BufferRecved, 0, sizeof(SlavePort.BufferRecved));
                Recv(&SlavePort, buffer, sizeof(buffer));
            }
            if(IsTimerWPRinging(&Timer1s)){
                RestartTimerWP(&Timer1s);

                //UART_SendChar('A');
                //UART_SendChar(UART_GetChar());
            }
            ReceivingTimerHandle(&SlavePort);
            SendingTimerHandle(&SlavePort);
            if (SlavePort.Status & PORT_RECEIVED_ALL) {
                //DEBUG_PRINTM(1, SlavePort.BufferRecved);
                //Write(&SlavePort, "Slave've got your msg!\n", 24);
                //memcpy(buffer, storedBuffer, sizeof(storedBuffer));
                Write(&SlavePort, SlavePort.BufferRecved, RECV_BUFFER_SIZE);
            }
        }
    }
}