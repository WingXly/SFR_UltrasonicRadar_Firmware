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
OrdinaryTimer * OrdinaryTimer_Instantiate(  TIM_HandleTypeDef * htim,
                                            float timer_clock_MHz );
PwmTimer * PwmTimer_Instantiate(TIM_HandleTypeDef * htim,
                                float timer_clock_MHz,
                                int PWM_generation_channel,
                                int compare_step );
CaptureTimer * CaptureTimer_Instantiate(TIM_HandleTypeDef * htim,
                                        float timer_clock_MHz,
                                        GPIO_TypeDef * trig_port,
                                        uint16_t trig_pin,
                                        int capture_channel_1,
                                        int capture_channel_2,
                                        HAL_TIM_ActiveChannel capture_active_channel );
void TimerEp_Timer_Free(Timer * this);

// 定时器基本方法
void TimerEp_SetCounter(Timer * this, int cycle);
int TimerEp_GetArr(Timer * this);
int TimerEp_GetPsc(Timer * this);
float TimerEp_GetPeriod(Timer * this);
float TimerEp_ms2cycle(Timer * this, float ms);
TIM_TypeDef * TimerEp_GetInstance(Timer * this);

// 普通定时器
void OrdinaryTimer_SetCycle(OrdinaryTimer * this, int cycle);
void OrdinaryTimer_CountDown(OrdinaryTimer * this);
void OrdinaryTimer_Reset(OrdinaryTimer * this);
int OrdinaryTimer_If_Reset(OrdinaryTimer * this);
int OrdinaryTimer_If_Counting(OrdinaryTimer * this);
int OrdinaryTimer_If_CountCplt(OrdinaryTimer * this);
void OrdinaryTimer_SetCycle_ms(OrdinaryTimer * this, float time_ms);

// PWM定时器
void PwmTimer_SetCompare(PwmTimer * this, int compare);
void PwmTimer_SetDuty(PwmTimer * this, float rate);
void PwmTimer_SetDuration(PwmTimer * this, float time);
int PwmTimer_GetCompare(PwmTimer * this);
float PwmTimer_GetDuty(PwmTimer * this);
float PwmTimer_GetDuration(PwmTimer * this);
float PwmTimer_GetCompare_AsServoAngle( PwmTimer * this,
                                        float minCompare, float maxCompare,
                                        float angleRange_degree );
float PwmTimer_GetDuty_AsServoAngle( PwmTimer * this,
                                     float minDuty_percent, float maxDuty_percent,
                                     float angleRange_degree );
int PwmTimer_NextCompare_Linear(PwmTimer * this,
                                float minDuty_percent, float maxDuty_percent );
void PwmTimer_SetCompare_Auto_Linear_ForthAndBack(  PwmTimer * this,
                                                    float minDuty_percent, float maxDuty_percent );

// 捕获定时器
void CaptureTimer_TriggerUp(CaptureTimer * this);
void CaptureTimer_TriggerDown(CaptureTimer * this);
volatile int CaptureTimer_ReadCounter_1(CaptureTimer * this);
volatile int CaptureTimer_ReadCounter_2(CaptureTimer * this);
HAL_TIM_ActiveChannel CaptureTimer_GetActiveChannel(CaptureTimer * this);
void CaptureTimer_SaveCounter_Auto_1(CaptureTimer * this);
void CaptureTimer_SaveCounter_Auto_2(CaptureTimer * this);
void CaptureTimer_SaveCounter_both(CaptureTimer * this);
int CaptureTimer_LoadCounter_1(CaptureTimer * this);
int CaptureTimer_LoadCounter_2(CaptureTimer * this);
void CaptureTimer_Mark_CaptureCplt(CaptureTimer * this, int is_it_complete);
int CaptureTimer_If_CaptureCplt(CaptureTimer * this);


#endif // __TIMER_EP_H