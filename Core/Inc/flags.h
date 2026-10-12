#ifndef __FLAGS__H_
#define __FLAGS__H_

#include <stdbool.h>

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
  TRIG_UP_AND_DOWN,       // <main> 上拉与下拉触发器
  MEASURING,              // <TIM1> 超声波测距仪正在测距
  PROCESSING,             // <main> 计算距离与处理OLED显示
  ROTATE,                 // <main> 发布舵机旋转指令
  COOLING                 // <TIM3> 冷却
} StateMachine_TypeDef;


// 系统参数与标志位
typedef struct {
    volatile StateMachine_TypeDef state_machine;
    volatile bool pause;
    volatile bool enable_oled;
    volatile bool oled_refresh_req;
    volatile bool oled_clear_req;
    volatile float cooling_time_ms;
} Flags_TypeDef;


// 事件参数
typedef struct {
    volatile int modify_step_optType;
    volatile int modify_step_value;
    volatile int modify_cooling_time_optType;
    volatile int modify_cooling_time_value;
} Params_TypeDef;


// 全局上下文实例
extern Flags_TypeDef flags;
extern Params_TypeDef params;

Usart * bluetooth = NULL;
LED * led_red   = NULL;
LED * led_green = NULL;
LED * led_blue  = NULL;
CaptureTimer  * capture_timer    = NULL;
OrdinaryTimer * main_cycle_timer = NULL;
PwmTimer      * servo_pwm_timer  = NULL;

#endif // __FLAGS__H_