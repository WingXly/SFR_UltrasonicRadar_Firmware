#ifndef __USART_EP_H_
#define __USART_EP_H_

#include "main.h"


typedef struct {
    UART_HandleTypeDef * huart;
    DMA_HandleTypeDef * DMA_handle_rx;
    DMA_HandleTypeDef * DMA_handle_tx;
    uint8_t * receive_data_p;
} Usart;


Usart * Usart_Instantiate(  UART_HandleTypeDef * huart,
                            DMA_HandleTypeDef * DMA_handle_rx,
                            DMA_HandleTypeDef * DMA_handle_tx,
                            uint8_t * receive_data_p );
void Usart_Free(Usart * this);


#endif