/**
 * @file bluetooth.c
 * @brief 有关蓝牙收发的代码
 * @anchor WingXly
 *
 * @attention
 * license MIT
 */

#include "bluetooth.h"
#include "main.h"
#include "flags.h"
#include <stdbool.h>
#include <stdint.h>

#define CMD_MAP_SIZE (sizeof(commands_map) / sizeof(commands_map[0]))

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


/*解析来自蓝牙的命令*/
CommandCode_TypeDef * Cmd_Parse(uint8_t * pReceiveData, uint8_t rx_size)
{
    // 初始化返回值
    static CommandCode_TypeDef cmd = {
        .type = CMD_DEFAULT,
        .argc = CMD_DEFAULT,
    };

    for(uint8_t i = 0; i < CMD_ARG_MAX; i++) {
        cmd.argv[i] = CMD_DEFAULT;
    }

    // 检验数据
    uint8_t len = pReceiveData[2];
    uint8_t len_total = len + 4; // 帧总长度理论值 = 2帧头 + len_tv + 1校验 + 1帧尾
    uint8_t xor = 0;

    // xor校验和：从 pReceiveData[2] 开始的 pReceiveData[2] 字节
    for (uint8_t i = 2; i < len + 2; i++) {
        xor ^= pReceiveData[i];
    }

    // 检验
    if (rx_size >= 4                           &&
        rx_size == len_total                   &&
        pReceiveData[0] == FRAME_HEADER_1      &&
        pReceiveData[1] == FRAME_HEADER_2      &&
        xor == pReceiveData[2 + len]           &&
        pReceiveData[len_total - 1] == FRAME_END)
    {
        // 检验通过
        cmd.type = (pReceiveData[3] << 8) + pReceiveData[4];

        // 逐个提取参数
        for(uint8_t loop = 0;
            len > 4 && loop < CMD_ARG_MAX;
            loop++)
        {
            cmd.argv[loop] = (pReceiveData[2 * loop + 5] << 8) + pReceiveData[2 * loop + 6];
            cmd.argc += 1;
            len -= 2;
        }
    }

    // 检验不通过，返回默认值
    return &cmd;
}


/*指令链表*/
static const struct
{
    uint16_t cmd_type;
    uint16_t  argc;
    bool (*handler)(const CommandCode_TypeDef * cmd);
}
commands_map[] =
{
    ///={       cmd_type           argc              handler            },
    { CMD_PAUSE_OR_START       , 1,      __Cmd_PauseOrStart__       },
    { CMD_OLED                 , 1,      __Cmd_Oled_OnOff__         },
    { CMD_SPEED                , 2,      __Cmd_Speed__              },
    { CMD_COOLING_TIME         , 2,      __Cmd_CoolingTime__        },
    { CMD_RESAMPLE             , 1,      __Cmd_Resample__           },
};


bool Cmd_Execute(const CommandCode_TypeDef * cmd)
{
    for (uint16_t i = 0; i < CMD_MAP_SIZE; i++) {
        if (commands_map[i].cmd_type == cmd->type && commands_map[i].handler != NULL)
        {
            //  检查参数个数
            if (commands_map[i].argc != cmd->argc) {
                put_queue_btSendError_awake(CMD_ERR_ARG_COUNT);
                return false;
            }

            // 参数个数正确
            return commands_map[i].handler(cmd);
        }
    }

    // 未知命令
    put_queue_btSendError_awake(CMD_ERR_UNKNOWN_CMD);
    return false;    
}


static bool __Cmd_PauseOrStart__(const CommandCode_TypeDef * cmd)
{
    if (cmd->argv[0] == 0) {
        pause = false;
        LED_on(led_green);
        LED_off(led_red);
    } else if (cmd->argv[0] == 1) {
        pause = true;
        LED_off(led_green);
        LED_on(led_red);
    } else {
        put_queue_btSendError_awake(CMD_ERR_INVALID_PARAM);
        return false;
    }
    return true;
}


static bool __Cmd_Oled_OnOff__(const CommandCode_TypeDef * cmd)
{
    if (cmd->argv[0] == 0) {
        enable_oled = false;
        oled_clear_req = true;
        LED_off(led_blue);
    } else if (cmd->argv[0] == 1) {
        enable_oled = true;
        oled_refresh_req = true;
        LED_on(led_blue);
    } else {
        put_queue_btSendError_awake(CMD_ERR_INVALID_PARAM);
        return false;
    }
    return true;
}


static bool __Cmd_Speed__(const CommandCode_TypeDef * cmd)
{
    if ( AutoCompare_setStep(auto_compare, cmd->argv[0], cmd->argv[1]) ) return true;
    return false;
}


static bool __Cmd_CoolingTime__(const CommandCode_TypeDef * cmd)
{
    float cooling_time_old = cooling_time_ms;

    if (cmd->argv[0] == 1) {
        cooling_time_ms = (float)cmd->argv[1] / 1000;
    } else if (cmd->argv[0] == 2) {
        cooling_time_ms += (float)cmd->argv[1] / 1000;
    } else if (cmd->argv[0] == 3) {
        cooling_time_ms -= (float)cmd->argv[1] / 1000;
    } else {
        put_queue_btSendError_awake(CMD_ERR_INVALID_PARAM);
        return false;
    }

    if (cooling_time_ms < 0.001f) {
        cooling_time_ms = cooling_time_old;
        put_queue_btSendError_awake(CMD_ERR_INVALID_PARAM);
        return false;
    }
    return true;
}


static bool __Cmd_Resample__(const CommandCode_TypeDef *cmd)
{
    if (cmd->argc < 1) return false;
    resample = cmd->argv[0];
    return true;
}