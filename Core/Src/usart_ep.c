/**
 * @file usart_ep.c
 * @brief 串口的面向对象封装
 * @anchor WingXly
 * @version 1.0
 *
 * @attention
 * license MIT
 *
 * @note
 * 使用流程:
 * 1. 在任意位置声明一个串口：`Usart * my_usart = NULL;`
 * 2. 在main()或其他函数中实例化，例如：`my_usart = Usart_Instantiate(&huart1, &hdma_usart1_rx, &hdma_usart1_tx);`
 * 3. 在实例化之后使用方法，具体详见本文内容
 * 4. 当无需使用一个实例时，将它销毁：`Usart_Free(my_usart);`
 *
 * @note
 * 为保持封装，务必要在一个函数中实例化，而不是在将对象声明为全局变量时直接实例化。
 *
 */
#include "usart_ep.h"
#include "stm32f1xx_hal_def.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


struct Usart {
    UART_HandleTypeDef * huart;
    DMA_HandleTypeDef * DMA_handle_rx;
    DMA_HandleTypeDef * DMA_handle_tx;
    uint8_t receive_data[20];
    int TX_CPLT;
    int RX_CPLT;
    int rx_size;
};


/*创建一个Usart实例*/
Usart * Usart_Instantiate(  UART_HandleTypeDef * huart,
                            DMA_HandleTypeDef * DMA_handle_rx,
                            DMA_HandleTypeDef * DMA_handle_tx )
{
    Usart * this = malloc(sizeof(Usart));
    if (!this) return NULL;
    this -> huart = huart;
    this -> DMA_handle_rx = DMA_handle_rx;
    this -> DMA_handle_tx = DMA_handle_tx;
    memset(&this->receive_data, 0, sizeof(this->receive_data));
    this -> TX_CPLT = 1;
    this -> RX_CPLT = 0;
    this -> rx_size = 0;
    return this;
}

void Usart_Free(Usart * this) { free(this); }


/*返回Usart的ReceiveData地址*/
uint8_t * Usart_pReceiveData(Usart * this)
{
    return this->receive_data;
}


/*返回Usart的huart地址*/
UART_HandleTypeDef * Usart_pHuart(Usart * this)
{
    return this->huart;
}


/*检测Usart是否接收完成*/
int Usart_is_RxCplt(Usart * this)
{
    return this->RX_CPLT;
}


/*检测Usart是否发送完成*/
int Usart_is_TxCplt(Usart * this)
{
    return this->TX_CPLT;
}


/*将Usart标记为接收完成或未完成*/
void Usart_mark_RxCplt(Usart * this, int Cplt)
{
    if (Cplt == 0) {
        this->RX_CPLT = 0;
    } else {
        this->RX_CPLT = 1;
    }
}


/*读取Usart接收到数据的长度（需要先标记）*/
int Usart_Get_RxSize(Usart * this)
{
    return this->rx_size;
}


/*标记Usart接收到数据的长度*/
void Usart_mark_RxSize(Usart * this, int size)
{
    this -> rx_size = size;
}


/*将Usart标记为发送完成或未完成*/
void Usart_mark_TxCplt(Usart * this, int Cplt)
{
    if (Cplt == 0) {
        this->TX_CPLT = 0;
    } else {
        this->TX_CPLT = 1;
    }
}


/*让Usart等待接收数据（异步传输，使用DMA，禁用传输过半中断）*/
void Usart_ReceiveToIdle_DMA_noHT(Usart * this)
{
    this->RX_CPLT = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(
          Usart_pHuart(this),
          Usart_pReceiveData(this),
          20
        );
    __HAL_DMA_DISABLE_IT(this->DMA_handle_rx, DMA_IT_HT);
}


/*让Usart等待接收数据（异步传输，使用DMA，启用传输过半中断）*/
void Usart_ReceiveToIdle_DMA_HT(Usart * this)
{
    this->RX_CPLT = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(
          Usart_pHuart(this),
          Usart_pReceiveData(this),
          20
        );
    __HAL_DMA_ENABLE_IT(this->DMA_handle_rx, DMA_IT_HT);
}


/*让Usart发送一段数据（异步传输，使用DMA，禁用传输过半中断）*/
void Usart_Transmit_DMA_noHT(Usart * this, uint8_t * pSendData_static, int size)
{
    this->TX_CPLT = 0;
    HAL_UART_Transmit_DMA(this->huart, pSendData_static, size);
    __HAL_DMA_DISABLE_IT(this->DMA_handle_tx, DMA_IT_HT);
}


/*让Usart发送一段数据（异步传输，使用DMA，启用传输过半中断）*/
void Usart_Transmit_DMA_HT(Usart * this, uint8_t * pSendData_static, int size)
{
    this->TX_CPLT = 0;
    HAL_UART_Transmit_DMA(this->huart, pSendData_static, size);
    __HAL_DMA_ENABLE_IT(this->DMA_handle_tx, DMA_IT_HT);
}


/*让Usart等待接收数据（异步传输，启用传输完成中断）*/
void Usart_ReceiveToIdle_IT(Usart * this)
{
    this->RX_CPLT = 0;
    HAL_UARTEx_ReceiveToIdle_IT(
          Usart_pHuart(this),
          Usart_pReceiveData(this),
          20
        );
}


/*让Usart发送一段数据（异步传输，启用传输完成中断）*/
void Usart_Transmit_IT(Usart * this, uint8_t * pSendData_static, int size)
{
    this->TX_CPLT = 0;
    HAL_UART_Transmit_IT(this->huart, pSendData_static, size);
}


/*让Usart发送一段数据（阻塞CPU，不触发传输完成中断）*/
HAL_StatusTypeDef Usart_Transmit(Usart * this, uint8_t * pSendData, int size, uint32_t timeOut)
{
    this->TX_CPLT = 0;
    return HAL_UART_Transmit(this->huart, pSendData, size, timeOut);
}