#include "usart.h"

void UART_Init(void)
{
    TRISCbits.TRISC6 = 0;  // TX as output
    TRISCbits.TRISC7 = 1;  // RX as input

    BRGH = 1;
    SPBRG = 25;  // For 9600 baud rate

    TXSTAbits.SYNC = 0;  // Asynchronous transmission
    RCSTAbits.SPEN = 1;  // Enable UART
    TXSTAbits.TXEN = 1;  // Enable transmitter
    RCSTAbits.CREN = 1;  // Enable receiver
    //PIE1bits.RCIE = 1;   // Enable receive interrupt
}

void UART_SendChar(char data)
{
    //while (PIE1bits.TXIE);  // Wait until the transmission buffer is empty
    //txBuffer = data;       // Place data in the buffer
    //PIE1bits.TXIE = 1;     // Enable transmission interrupt
    
    while(TRMT==0);
    TXREG = data;
}

uint8_t UART_GetChar(void)
{
    while(RCIF == 0);
    if(OERR){
        CREN = 0;
        CREN = 1;
    }
    return RCREG;
}

void USART_ReceiveINTEnable(void)
{
    //CREN = 1;
    PIE1bits.RCIE = 1;
    //RCIF = 0;
}

void USART_ReceiveINTDisable(void)
{
    //CREN = 0;
    PIE1bits.RCIE = 0;
    //RCIF = 0;
}

void USART_TransmitINTEnable(void)
{
    //TXEN = 1;
    //PIE1bits.TXIE = 1;
    //TXIF = 0;
}

void USART_TransmitINTDisable(void)
{
    //TXEN = 0;
    //PIE1bits.TXIE = 0;
    //TXIF = 0;
}

uint8_t USART_GetDataFromReceiveISR(void)
{
    //if(RCSTAbits.OERR){
    //    RCSTAbits.CREN = 0;
    //    RCSTAbits.CREN = 1;
    //}
    //RCSTAbits.FERR == ?
    return RCREG;  //Read received data
}

int USART_Transmit(uint8_t data)
{
    //TXSTAbits.TXEN = 1;
    if(TRMT == 1) //?!
        TXREG = data;
    return 0;
}