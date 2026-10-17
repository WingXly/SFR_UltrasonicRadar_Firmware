#ifndef __USART_EP_H_
#define __USART_EP_H_

#include "main.h"


typedef struct Usart Usart;

Usart * Usart_Instantiate(UART_HandleTypeDef * huart,
                          DMA_HandleTypeDef * DMA_handle_rx,
                          DMA_HandleTypeDef * DMA_handle_tx);
void Usart_Free(Usart * this);

uint8_t * Usart_Get_pReceiveData(Usart * this);
UART_HandleTypeDef * Usart_Get_pHuart(Usart * this);

bool Usart_is_RxCplt(Usart * this);
bool Usart_is_TxCplt(Usart * this);

void Usart_mark_RxCplt(Usart * this, bool is_it_cplt);
void Usart_mark_RxCplt_withSize(Usart * this, uint16_t size);
void Usart_mark_RxSize(Usart * this, uint16_t size);
uint16_t Usart_Get_RxSize(Usart * this);

void Usart_mark_TxCplt(Usart * this, bool is_it_cplt);

HAL_StatusTypeDef Usart_ReceiveToIdle_DMA_noHT(Usart * this);
HAL_StatusTypeDef Usart_ReceiveToIdle_DMA_HT(Usart * this);

HAL_StatusTypeDef Usart_Transmit_DMA_noHT(Usart * this, uint8_t * pSendData_static, int size);
HAL_StatusTypeDef Usart_Transmit_DMA_HT(Usart * this, uint8_t * pSendData_static, int size);

HAL_StatusTypeDef Usart_ReceiveToIdle_IT(Usart * this);

HAL_StatusTypeDef Usart_Transmit_IT(Usart * this, uint8_t * pSendData_static, int size);
HAL_StatusTypeDef Usart_Transmit(Usart * this, uint8_t * pSendData, int size, uint32_t timeOut);

#endif
