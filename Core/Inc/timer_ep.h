#ifndef __TIMER_EP_H
#define __TIMER_EP_H
#include "main.h"
#include "stm32f1xx_hal.h"


// Timer类声明:
typedef struct Timer Timer;
typedef struct OrdinaryTimer OrdinaryTimer;
typedef struct PwmTimer PwmTimer;
typedef struct CaptureTimer CaptureTimer;


// 初始化与释放
OrdinaryTimer * TimerEp_OrdinaryTimer_Instantiate(TIM_HandleTypeDef * htim,
                                                  float timer_clock_MHz
                                                  );
PwmTimer * TimerEp_PwmTimer_Instantiate(TIM_HandleTypeDef * htim,
                                        float timer_clock_MHz,
                                        int PWM_generation_channel
                                        );
CaptureTimer * TimerEp_CaptureTimer_Instantiate(TIM_HandleTypeDef * htim,
                                                float timer_clock_MHz,
                                                GPIO_TypeDef * trig_port,
                                                uint16_t trig_pin,
                                                int capture_upEdge_channel,
                                                int capture_downEdge_channel,
                                                HAL_TIM_ActiveChannel capture_active_channel
                                                );
void TimerEp_Timer_Free(Timer * this);

// 定时器基本方法
void TimerEp_SetCounter(Timer * this, int cycle);
int TimerEp_GetArr(Timer * this);
int TimerEp_GetPsc(Timer * this);
float TimerEp_GetPeriod(Timer * this);
TIM_TypeDef * TimerEp_GetInstance(Timer * this);

// 普通定时器
void TimerEp_OrdinaryTimer_SetCycle(OrdinaryTimer * this, int cycle);
void TimerEp_OrdinaryTimer_CountDown(OrdinaryTimer * this);
void TimerEp_OrdinaryTimer_Reset(OrdinaryTimer * this);
int TimerEp_OrdinaryTimer_If_Reset(OrdinaryTimer * this);
int TimerEp_OrdinaryTimer_If_Counting(OrdinaryTimer * this);
int TimerEp_OrdinaryTimer_If_AC(OrdinaryTimer * this);

// PWM定时器
void TimerEp_PwmTimer_SetCompare(PwmTimer * this, int compare);
void TimerEp_PwmTimer_SetCompareRate(PwmTimer * this, float rate);
void TimerEp_PwmTimer_SetCompareTime(PwmTimer * this, float time);
int TimerEp_PwmTimer_GetCompare(PwmTimer * this);
float TimerEp_PwmTimer_GetCompareRate(PwmTimer * this);
float TimerEp_PwmTimer_GetCompareTime(PwmTimer * this);

// 捕获定时器
void TimerEp_CaptureTimer_TriggerUp(CaptureTimer * this);
void TimerEp_CaptureTimer_TriggerDown(CaptureTimer * this);
volatile int TimerEp_CaptureTimer_ReadUpEdge(CaptureTimer * this);
volatile int TimerEp_CaptureTimer_ReadDownEdge(CaptureTimer * this);
HAL_TIM_ActiveChannel TimerEp_CaptureTimer_GetActiveChannel(CaptureTimer * this);


#endif // __TIMER_EP_H