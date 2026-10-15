#ifndef __FLAGS__H_
#define __FLAGS__H_

#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "usart_ep.h"
#include "led.h"
#include "timer_ep.h"


// 声速
#define SPEED_OF_SOUND_m_s 340.0f

// HC-SR04的触发用时
#define TRIG_DURATION_ms 0.01f

// PWM舵机最小占空比
#define PWMSERVO_MIN_COMPARE 50

// PWM舵机最大占空比
#define PWMSERVO_MAX_COMPARE 250

// PWM舵机的角度范围
#define PWMSERVO_ANGLE_RANGE 180.0f


// 状态机
typedef enum {
  INIT,                   // 初始化
  PAUSE_AND_EVENT_HANDLE, // 暂停与事件处理
  MEASURING,              // 超声波测距仪正在测距
  PROCESSING,             // 计算距离与处理OLED显示
  ROTATE,                 // 发布舵机旋转指令
  COOLING                 // 冷却
} StateMachine_TypeDef;


/*全局标识符，采用extern防止暂定定义*/

extern StateMachine_TypeDef state_machine;

extern bool pause;
extern bool enable_oled;
extern bool oled_refresh_req;
extern bool oled_clear_req;
extern float cooling_time_ms;
extern uint8_t resample;
extern uint8_t resample_remain;

extern Usart * bluetooth;
extern LED * led_red;
extern LED * led_green;
extern LED * led_blue;
extern CaptureTimer  * capture_timer;
extern OrdinaryTimer * main_cycle_timer;
extern PwmTimer      * servo_pwm_timer;

#endif // __FLAGS__H_