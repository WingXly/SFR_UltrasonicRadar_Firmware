#include "bluetooth.h"
#include <stdint.h>
#include <string.h>
#include "led.h"
#include "usart_ep.h"
#include "stm32f1xx_hal_uart.h"


/* 以下变量定义在 main.c 中*/
extern Usart * BlueTooth;
extern volatile int PAUSE;
extern volatile int ENABLE_OLED;
extern volatile int OLED_CLEAR_REQUEST;
extern volatile int OLED_REFRESH_REQUEST;
extern volatile float cooling_time_ms;
extern volatile int MODIFY_STEP_OPT_TYPE;
extern volatile int MODIFY_STEP_VALUE;
extern LED * LedRed;
extern LED * LedGreen;
extern LED * LedBlue;


/*串口Tx回调函数*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Usart_pHuart(BlueTooth)) {
        Usart_Mark_TxCplt(BlueTooth, 1);
    }
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == Usart_pHuart(BlueTooth)) {
        bluetooth_msg_handler(Usart_pReceiveData(BlueTooth), Size);
        Usart_ReceiveToIdle_DMA_noHT(BlueTooth);
    }
}

/*
将距离和角度通过串口发送至电脑。采用二进制帧结构发布通知：

uint8_t sendMsg[9]:
帧头         长度    角度     距离        校验      帧尾
0xAA 0x55    N      int16    uint16      XOR      0x0D
0    1       2      3   4    5    6      7        8

角度单位：度 -> 0.01度
距离单位：mm -> mm
*/
void send_auto_bluetooth(float distance_mm, float angle_deg)
{
    if ( !Usart_If_TxCplt(BlueTooth) ) return;
    static uint8_t sendMsg[9] = "";

    sendMsg[0] = FRAME_HEADER_1;
    sendMsg[1] = FRAME_HEADER_2;
    sendMsg[2] = 5;                                 // 长度
    // 以0.01度为单位发送角度数据
    sendMsg[3] = (int16_t)(angle_deg * 100) >> 8;   // 角度高字节
    sendMsg[4] = (int16_t)(angle_deg * 100) & 0xFF; // 角度低字节
    // 以1mm为单位发送距离数据
    sendMsg[5] = (uint16_t)distance_mm >> 8;        // 距离高字节
    sendMsg[6] = (uint16_t)distance_mm & 0xFF;      // 距离低字节
    sendMsg[7] = sendMsg[2] ^ sendMsg[3] ^ sendMsg[4] ^ sendMsg[5] ^ sendMsg[6]; // 校验
    sendMsg[8] = FRAME_END;                              // 帧尾

    Usart_Mark_TxCplt(BlueTooth, 0);
    Usart_Transmit_DMA_noHT(BlueTooth, sendMsg, 9);
}


/*解析来自蓝牙的命令*/
void bluetooth_msg_handler(uint8_t * cmd, uint16_t len)
{
    if (len < 4) return;
    if (cmd[0] != FRAME_HEADER_1 || cmd[1] != FRAME_HEADER_2) return;

    uint8_t n = cmd[2];

    // len =?= 帧总长度 = 2帧头 + n字节 + 1校验 + 1帧尾
    uint16_t total_len = (uint16_t)n + 4U;
    if (len < total_len) return;
    if (cmd[total_len - 1] != FRAME_END) return;

    // 校验范围：cmd[2] 开始的n字节
    uint8_t checksum = 0;
    for (int i = 0; i < n; i++) checksum ^= cmd[2 + i];
    if (checksum != cmd[2 + n]) return;

    // 检验通过
    if (n == 5) {
        uint16_t cmd_type = (cmd[3] << 8) + cmd[4];
        uint16_t para = (cmd[5] << 8) + cmd[6];
        command_handler_1para(cmd_type, para);
    }
    else if (n == 7) {
        uint16_t cmd_type = (cmd[3] << 8) + cmd[4];
        uint16_t para1 = (cmd[5] << 8) + cmd[6];
        uint16_t para2 = (cmd[7] << 8) + cmd[8];
        command_handler_2para(cmd_type, para1, para2);
    }
}

/*执行来自蓝牙的单参数命令（需要先解析）*/
void command_handler_1para(uint16_t cmd_type, uint16_t para)
{
    if (cmd_type == 0)      // 设置机器是否暂停
    {
        if (para == 0)      // 解除暂停
        {
            PAUSE = 0;
            LED_on(LedGreen);
            LED_off(LedRed);
        }
        else if (para == 1) // 暂停
        {
            PAUSE = 1;
            LED_off(LedGreen);
            LED_on(LedRed);
        }
    }
    else if (cmd_type == 1) // 设置OLED屏幕的开或关
    {
        if (para == 0)      // 关闭OLED
        {
            ENABLE_OLED = 0;
            OLED_CLEAR_REQUEST = 1;
            LED_off(LedBlue);
            
        }
        else if (para == 1) // 开启OLED
        {
            ENABLE_OLED = 1;
            OLED_REFRESH_REQUEST = 1;
            LED_on(LedBlue);
        }
    }
    else                    // 报错
    {
        send_error_bluetooth();
    }
}


/*执行来自蓝牙的双参数命令（需要先解析）*/
void command_handler_2para(uint16_t cmd_type, uint16_t para1, uint16_t para2)
{
    if (cmd_type == 2)      // 设置 compare_step。将para一比一转换
    {
        // 只发送修改请求，具体修改逻辑在main()中实现
        MODIFY_STEP_OPT_TYPE = para1;
        MODIFY_STEP_VALUE = para2;
    }
    else if (cmd_type == 3) // 设置 cooling_time_ms。将para除以1000后转换
    {
        float cooling_time_ms_old = cooling_time_ms;
        if (para1 == 1) {        // set
            cooling_time_ms = (float)para2 / 1000;
        } else if (para1 == 2) { // add
            cooling_time_ms += (float)para2 / 1000;
        } else if (para1 == 3) { // remove
            cooling_time_ms -= (float)para2 / 1000;
        }

        if (cooling_time_ms < 0.001) {
            cooling_time_ms = cooling_time_ms_old;
            send_error_bluetooth();
        }
    }
    else                    // 报错
    {
        send_error_bluetooth();
    }

}


/*向蓝牙通知一个错误*/
void send_error_bluetooth()
{
    static char sendMsg[50] = "Error";
    Usart_Mark_TxCplt(BlueTooth, 0);
    Usart_Transmit_DMA_noHT(BlueTooth, (uint8_t*)sendMsg, strlen(sendMsg));
}