#ifndef __USART_EP_H_
#define __USART_EP_H_

#include "main.h"


typedef struct Usart Usart;

Usart * Usart_Instantiate(  UART_HandleTypeDef * huart,
                            DMA_HandleTypeDef * DMA_handle_rx,
                            DMA_HandleTypeDef * DMA_handle_tx );
void Usart_Free(Usart * this);

uint8_t * Usart_pReceiveData(Usart * this);
UART_HandleTypeDef * Usart_pHuart(Usart * this);
void Usart_ReceiveToIdle_DMA_noHT(Usart * this);
void Usart_ReceiveToIdle_DMA_HT(Usart * this);
void Usart_Transmit_DMA_noHT(Usart * this, uint8_t * pSendData_static, int size);
void Usart_Transmit_DMA_HT(Usart * this, uint8_t * pSendData_static, int size);
void Usart_ReceiveToIdle_IT(Usart * this);
void Usart_Transmit_IT(Usart * this, uint8_t * pSendData_static, int size);
HAL_StatusTypeDef Usart_Transmit(Usart * this, uint8_t * pSendData, int size, uint32_t timeOut);

#endif