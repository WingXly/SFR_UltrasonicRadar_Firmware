#include "usart_ep.h"
#include <stdlib.h>


Usart * Usart_Instantiate(  UART_HandleTypeDef * huart,
                            DMA_HandleTypeDef * DMA_handle_rx,
                            DMA_HandleTypeDef * DMA_handle_tx,
                            uint8_t * receive_data_p )
{
    Usart * this = malloc(sizeof(Usart));
    if (!this) return NULL;
    this->huart = huart;
    this->DMA_handle_rx = DMA_handle_rx;
    this->DMA_handle_tx = DMA_handle_tx;
    this->receive_data_p = receive_data_p;
    return this;
}
void Usart_Free(Usart * this) { free(this); }


