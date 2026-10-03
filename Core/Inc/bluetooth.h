#ifndef __BLUETOOTH_H_
#define __BLUETOOTH_H_

#include "main.h"

/* 雷达二进制帧协议 */
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_END     0x0D


void command_handler_1para(uint16_t cmd_type, uint16_t cmd_para);
void command_handler_2para(uint16_t cmd_type, uint16_t para1, uint16_t para2);
void send_auto_bluetooth(float distance_mm, float angle_deg);
void bluetooth_msg_handler(uint8_t * cmd, uint16_t len);
void send_error_bluetooth();

#endif