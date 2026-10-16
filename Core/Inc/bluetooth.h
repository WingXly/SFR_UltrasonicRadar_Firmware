#ifndef __BLUETOOTH_H_
#define __BLUETOOTH_H_

#include "main.h"
#include "usart_ep.h"

/* 二进制帧协议 */
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_END      0x0D


void cmd_executor_1para(uint16_t cmd_type, uint16_t cmd_para);
void cmd_executor_2para(uint16_t cmd_type, uint16_t para1, uint16_t para2);
void send_sensor_data(Usart * who_send_data, float distance_mm, float angle_deg);
void cmd_handler(Usart * from);
void send_error(Usart * who_send_data, uint16_t error_code);

#endif