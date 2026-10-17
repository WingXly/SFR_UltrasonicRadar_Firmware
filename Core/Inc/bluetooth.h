#ifndef __BLUETOOTH_H_
#define __BLUETOOTH_H_

#include "main.h"
#include "usart_ep.h"

/* 二进制帧协议 */
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_END      0x0D

/*错误码*/
#define ERR_INVALID_PARAM 0x00
#define ERR_ARG_COUNT 0x01
#define ERR_UNKNOWN_CMD 0x02

// 最大参数个数
#define CMD_ARG_MAX 2

// 缺省值
#define CMD_DEFAULT 65535

/*指令类型表*/
typedef enum {
    CMD_PAUSE_OR_START = 0,
    CMD_OLED           = 1,
    CMD_SPEED          = 2,
    CMD_COOLING_TIME   = 3,
    CMD_RESAMPLE       = 4
} CommandType_TypeDef;

typedef struct {
    CommandType_TypeDef type;   // 命令类型
    uint16_t argc;              // 参数个数
    uint16_t argv[CMD_ARG_MAX]; // 参数列表
} CommandCode_TypeDef;

CommandCode_TypeDef * Cmd_Parse(Usart * from);
bool Cmd_Execute(const CommandCode_TypeDef * cmd);

static bool __Cmd_PauseOrStart__(const CommandCode_TypeDef * cmd);
static bool __Cmd_Oled_OnOff__(const CommandCode_TypeDef * cmd);
static bool __Cmd_Speed__(const CommandCode_TypeDef * cmd);
static bool __Cmd_CoolingTime__(const CommandCode_TypeDef * cmd);
static bool __Cmd_Resample__(const CommandCode_TypeDef *cmd);

void send_sensor_data(Usart * who_send_data, float distance_mm, float angle_deg);
void send_error(Usart * who_send_data, uint16_t error_code);


#endif
