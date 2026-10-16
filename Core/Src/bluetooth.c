/**
 * @file bluetooth.c
 * @brief 有关蓝牙收发的代码
 * @anchor WingXly
 *
 * @attention
 * license MIT
 */

#include "main.h"
#include "flags.h"


/*解析来自蓝牙的命令*/
void cmd_handler(Usart * from)
{
    uint8_t len = Usart_Get_RxSize(from);
    uint8_t * cmd = Usart_GetpReceiveData(from);

    if (len < 4) return;
    if (cmd[0] != FRAME_HEADER_1 || cmd[1] != FRAME_HEADER_2) return;

    uint8_t n = cmd[2];

    // len =?= 帧总长度 = 2帧头 + n字节 + 1校验 + 1帧尾
    uint16_t total_len = (uint16_t)n + 4U;
    if (len < total_len) return;
    if (cmd[total_len - 1] != FRAME_END) return;

    // 校验范围：cmd[2] 开始的n字节
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < n; i++) checksum ^= cmd[2 + i];
    if (checksum != cmd[2 + n]) return;

    // 检验通过
    uint16_t cmd_type = (cmd[3] << 8) + cmd[4];
    uint16_t para1 = (cmd[5] << 8) + cmd[6];
    queue_btRxCommand_put(&cmd_type);
    queue_btRxCommand_put(&para1);
    if (n == 7) {
        uint16_t para2 = (cmd[7] << 8) + cmd[8];
        queue_btRxCommand_put(&para2);
    }
    static uint16_t end_flag = DEFAULT_CMD_CODE; // DEFAULT_CMD_CODE表示一个命令的结束
    queue_btRxCommand_put(&end_flag);
}


/*执行来自蓝牙的单参数命令（需要先解析）*/
void cmd_executor_1para(uint16_t cmd_type, uint16_t para)
{
    if (cmd_type == 0)      // 设置机器是否暂停
    {
        if (para == 0)      // 解除暂停
        {
            pause = false;
            LED_on(led_green);
            LED_off(led_red);
        }
        else if (para == 1) // 暂停
        {
            pause = true;
            LED_off(led_green);
            LED_on(led_red);
        }
        else
        {
            put_queue_btSendError_awake(0x00);
        }
    }
    else if (cmd_type == 1) // 设置OLED屏幕的开或关
    {
        if (para == 0)      // 关闭OLED
        {
            enable_oled = false;
            oled_clear_req = true;
            LED_off(led_blue);
            
        }
        else if (para == 1) // 开启OLED
        {
            enable_oled = true;
            oled_refresh_req = true;
            LED_on(led_blue);
        }
        else
        {
            put_queue_btSendError_awake(0x00);
        }
    }
    else if (cmd_type == 4)
    {
        resample = para;
    }
    else
    {
        // 输入的参数个数与指令不匹配
        put_queue_btSendError_awake(0x01);
    }
}


/*执行来自蓝牙的双参数命令（需要先解析）*/
void cmd_executor_2para(uint16_t cmd_type, uint16_t para1, uint16_t para2)
{
    if (cmd_type == 2)      // 设置 compare_step。将para一比一转换
    {
        AutoCompare_setStep(
            auto_compare,
            para1,
            para2
        );
    }
    else if (cmd_type == 3) // 设置 cooling_time_ms。将para除以1000后转换
    {
        float cooling_time_old = cooling_time_ms;
          
        if (para1 == 1) {        // set
            cooling_time_ms = (float)para2 / 1000;
        } else if (para1 == 2) { // add
            cooling_time_ms += (float)para2 / 1000;
        } else if (para1 == 3) { // remove
            cooling_time_ms -= (float)para2 / 1000;
        }

        // 不允许冷却时间小于1us
        if (cooling_time_ms < 0.001) {
            cooling_time_ms = cooling_time_old;
            put_queue_btSendError_awake(0x00);
        }
    }
    else
    {
        // 输入的参数个数与指令不匹配
        put_queue_btSendError_awake(0x01);
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
void send_sensor_data(Usart * who_send_data, float distance_mm, float angle_deg)
{
    static uint8_t sendMsg[9] = "";

    sendMsg[0] = FRAME_HEADER_1;                                                 // 帧头1
    sendMsg[1] = FRAME_HEADER_2;                                                 // 帧头2
    sendMsg[2] = 5;                                                              // 从有效数据开始到校验和位的长度
    // 以0.01度为单位发送角度数据
    sendMsg[3] = (int16_t)(angle_deg * 100) >> 8;                                // 角度高字节
    sendMsg[4] = (int16_t)(angle_deg * 100) & 0xFF;                              // 角度低字节
    // 以1mm为单位发送距离数据
    sendMsg[5] = (uint16_t)distance_mm >> 8;                                     // 距离高字节
    sendMsg[6] = (uint16_t)distance_mm & 0xFF;                                   // 距离低字节
    sendMsg[7] = sendMsg[2] ^ sendMsg[3] ^ sendMsg[4] ^ sendMsg[5] ^ sendMsg[6]; // 校验
    sendMsg[8] = FRAME_END;                                                      // 帧尾

    Usart_Transmit_DMA_noHT(who_send_data, sendMsg, 9);
}


/*向蓝牙通知一个错误*/
void send_error(Usart * who_send_data, uint16_t error_code)
{
    static uint8_t sendMsg[9] = "";

    sendMsg[0] = FRAME_HEADER_1,
    sendMsg[1] = FRAME_HEADER_2,
    sendMsg[2] = 5,
    sendMsg[3] = 0xFF;
    sendMsg[4] = 0xFF,
    sendMsg[5] = error_code >> 8;
    sendMsg[6] = error_code & 0xFF;
    sendMsg[7] = sendMsg[2] ^ sendMsg[3] ^ sendMsg[4] ^ sendMsg[5] ^ sendMsg[6];
    sendMsg[8] = FRAME_END;

    Usart_Transmit_DMA_noHT(who_send_data, sendMsg, 9);
}