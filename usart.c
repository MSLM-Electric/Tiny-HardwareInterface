#include "usart.h"

void UART_Init(void)
{
#ifndef DEBUG_ON_VS
    TRISCbits.TRISC6 = 0;  // TX as output
    TRISCbits.TRISC7 = 1;  // RX as input

    BRGH = 1;
    SPBRG = 25;  // For 9600 baud rate

    TXSTAbits.SYNC = 0;  // Asynchronous transmission
    RCSTAbits.SPEN = 1;  // Enable UART
    TXSTAbits.TXEN = 1;  // Enable transmitter
    RCSTAbits.CREN = 1;  // Enable receiver
    //PIE1bits.RCIE = 1;   // Enable receive interrupt
#endif // !DEBUG_ON_VS
}

void UART_SendChar(char data)
{
#ifndef DEBUG_ON_VS
    //while (PIE1bits.TXIE);  // Wait until the transmission buffer is empty
    //txBuffer = data;       // Place data in the buffer
    //PIE1bits.TXIE = 1;     // Enable transmission interrupt
    
    while(TRMT==0);
    TXREG = data;
#endif // !DEBUG_ON_VS
}

uint8_t UART_GetChar(void)
{
#ifndef DEBUG_ON_VS
    while(RCIF == 0);
    if(OERR){
        CREN = 0;
        CREN = 1;
    }
    return RCREG;
#else
    return '0';
#endif // !DEBUG_ON_VS
}

void USART_ReceiveINTEnable(void)
{
#ifndef DEBUG_ON_VS
    //CREN = 1;
    PIE1bits.RCIE = 1;
    //RCIF = 0;
#endif // !DEBUG_ON_VS
}

void USART_ReceiveINTDisable(void)
{
#ifndef DEBUG_ON_VS
    //CREN = 0;
    PIE1bits.RCIE = 0;
    //RCIF = 0;
#endif // !DEBUG_ON_VS
}

void USART_TransmitINTEnable(void)
{
#ifndef DEBUG_ON_VS
    //TXEN = 1;
    //PIE1bits.TXIE = 1;
    //TXIF = 0;
#endif // !DEBUG_ON_VS
}

void USART_TransmitINTDisable(void)
{
#ifndef DEBUG_ON_VS
    //TXEN = 0;
    //PIE1bits.TXIE = 0;
    //TXIF = 0;
#endif // !DEBUG_ON_VS
}

uint8_t USART_GetDataFromReceiveISR(void)
{
#ifndef DEBUG_ON_VS
    //if(RCSTAbits.OERR){
    //    RCSTAbits.CREN = 0;
    //    RCSTAbits.CREN = 1;
    //}
    //RCSTAbits.FERR == ?
    return RCREG;  //Read received data
#else
    return 0;
#endif // !DEBUG_ON_VS
}

int USART_Transmit(uint8_t data)
{
#ifndef DEBUG_ON_VS
    //TXSTAbits.TXEN = 1;
    if(TRMT == 1) //?!
        TXREG = data;
#endif // !DEBUG_ON_VS
    return 0;
}

int USART_TXRXsimpleCheck(char data)
{
    UART_SendChar(data);
    UART_SendChar(UART_GetChar());
}