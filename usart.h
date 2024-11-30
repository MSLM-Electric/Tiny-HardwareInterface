#ifndef USART_H
#define USART_H
#ifndef DEBUG_ON_VS
#include <xc.h>
#else
#include "vssimulationport.h"
#endif // !DEBUG_ON_VS
#include <stdint.h>

void UART_Init(void);
void UART_SendChar(char data);
uint8_t UART_GetChar(void);
void USART_ReceiveINTEnable(void);
void USART_ReceiveINTDisable(void);
void USART_TransmitINTEnable(void);
void USART_TransmitINTDisable(void);
uint8_t USART_GetDataFromReceiveISR(void);
int USART_Transmit(uint8_t data);

#endif // !USART_H

