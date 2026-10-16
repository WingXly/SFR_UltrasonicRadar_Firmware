#ifndef __TIMER_EP_H
#define __TIMER_EP_H
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>


// Timer类声明
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
static void __TimerEp_SetCounter__(Timer * this, int cycle);
static uint16_t __TimerEp_GetArr__(Timer * this);
static uint16_t __TimerEp_GetPsc__(Timer * this);
float TimerEp_GetPeriod(Timer * this);
float TimerEp_ms2cycle(Timer * this, float time_ms);
TIM_TypeDef * TimerEp_GetInstance(Timer * this);

// 普通定时器
bool OrdinaryTimer_Init_noIT(OrdinaryTimer * this);
bool OrdinaryTimer_Init_IT(OrdinaryTimer * this);
void OrdinaryTimer_SetCycle(OrdinaryTimer * this, int cycle);
void OrdinaryTimer_CountDown(OrdinaryTimer * this);
void OrdinaryTimer_Reset(OrdinaryTimer * this);
bool OrdinaryTimer_is_Reset(OrdinaryTimer * this);
bool OrdinaryTimer_is_Counting(OrdinaryTimer * this);
bool OrdinaryTimer_is_CountCplt(OrdinaryTimer * this);
void OrdinaryTimer_SetCycle_ms(OrdinaryTimer * this, float time_ms);

// PWM定时器
bool PwmTimer_Init(PwmTimer * this);
void PwmTimer_SetCompare(PwmTimer * this, uint16_t compare);
void PwmTimer_SetDuty(PwmTimer * this, float duty);
void PwmTimer_SetDuration(PwmTimer * this, float time_ms);
uint16_t PwmTimer_GetCompare(PwmTimer * this);
float PwmTimer_GetDuty(PwmTimer * this);
float PwmTimer_GetDuration_ms(PwmTimer * this);
// PWm定时器额外方法
float PwmTimerEx_GetCompare_AsServoAngle( PwmTimer * this,
                                        float minCompare, float maxCompare,
                                        float angleRange_degree );
float PwmTimerEx_GetDuty_AsServoAngle( PwmTimer * this,
                                     float minDuty_percent, float maxDuty_percent,
                                     float angleRange_degree );
int PwmTimerEx_NextCompare_Linear(PwmTimer * this,
                                float minDuty_percent, float maxDuty_percent );
void PwmTimerEx_SetCompare_Auto_Linear_ForthAndBack(  PwmTimer * this,
                                                    float minDuty_percent, float maxDuty_percent );

// 捕获定时器
bool CaptureTimer_Init(CaptureTimer * this);
void CaptureTimer_Reset(CaptureTimer * this);
void CaptureTimer_TriggerUp(CaptureTimer * this);
void CaptureTimer_TriggerDown(CaptureTimer * this);
static uint16_t __CaptureTimer_ReadCounter_1__(CaptureTimer * this);
static uint16_t __CaptureTimer_ReadCounter_2__(CaptureTimer * this);
HAL_TIM_ActiveChannel CaptureTimer_GetActiveChannel(CaptureTimer * this);
void CaptureTimer_SaveCounter_Auto_1(CaptureTimer * this);
void CaptureTimer_SaveCounter_Auto_2(CaptureTimer * this);
void CaptureTimer_SaveCounter_both(CaptureTimer * this);
uint16_t CaptureTimer_LoadCounter_1(CaptureTimer * this);
uint16_t CaptureTimer_LoadCounter_2(CaptureTimer * this);
static void __CaptureTimer_mark_CaptureCplt__(CaptureTimer * this, bool is_it_complete);
bool CaptureTimer_is_CaptureCplt(CaptureTimer * this);


#endif // __TIMER_EP_H