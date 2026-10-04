/**
 * @file timer_ep.c
 * @brief 定时器的面向对象封装
 * @anchor WingXly
 * @version 1.0
 * @date 2026-10-03
 * @license MIT License
 */
#include "timer_ep.h"
#include "stm32f1xx_hal_tim.h"
#include <stdint.h>
#include <stdlib.h>


struct Timer {
    TIM_HandleTypeDef * htim;
    float timer_clock_MHz;
    float period;
};

struct OrdinaryTimer{
    Timer timer_base;
    volatile int AC_flag;
    volatile int cycle;
};

struct PwmTimer {
    Timer timer_base;
    int PWM_compare;
    int PWM_generation_channel;
};

struct CaptureTimer {
    Timer timer_base;
    GPIO_TypeDef * trig_port;
    uint16_t trig_pin;
    int capture_channel_1;
    int capture_channel_2;
    HAL_TIM_ActiveChannel capture_active_channel;
    volatile int capture_count_1;
    volatile int capture_count_2;
    volatile int CAPTURE_DONE;
};


/*创建一个普通定时器*/
OrdinaryTimer * OrdinaryTimer_Instantiate(  TIM_HandleTypeDef * htim,
                                            float timer_clock_MHz )
{
    OrdinaryTimer * this = malloc(sizeof(OrdinaryTimer));
    if (!this) return NULL;
    this -> timer_base.htim = htim;
    this -> timer_base.timer_clock_MHz = timer_clock_MHz;
    this -> AC_flag = 0;
    this -> cycle = 0;
    this -> timer_base.period = TimerEp_GetPeriod((Timer *)this);
    return this;
}


/*创建一个PWM定时器*/
PwmTimer * PwmTimer_Instantiate(TIM_HandleTypeDef * htim,
                                float timer_clock_MHz,
                                int PWM_generation_channel )
{
    PwmTimer * this = malloc(sizeof(PwmTimer));
    if (!this) return NULL;
    this -> timer_base.htim = htim;
    this -> timer_base.timer_clock_MHz = timer_clock_MHz;
    this -> PWM_compare = 0;
    this -> PWM_generation_channel = PWM_generation_channel;
    this -> timer_base.period = TimerEp_GetPeriod((Timer *)this);
    return this;
}


/*创建一个捕获定时器*/
CaptureTimer * CaptureTimer_Instantiate(TIM_HandleTypeDef * htim,
                                        float timer_clock_MHz,
                                        GPIO_TypeDef * trig_port,
                                        uint16_t trig_pin,
                                        int capture_channel_1,
                                        int capture_channel_2,
                                        HAL_TIM_ActiveChannel capture_active_channel )
{
    CaptureTimer * this = malloc(sizeof(CaptureTimer));
    if (!this) return NULL;
    this -> timer_base.htim = htim;
    this -> timer_base.timer_clock_MHz = timer_clock_MHz;
    this -> trig_port = trig_port;
    this -> trig_pin = trig_pin;
    this -> capture_channel_1 = capture_channel_1;
    this -> capture_channel_2 = capture_channel_2;
    this -> capture_active_channel = capture_active_channel;
    this -> timer_base.period = TimerEp_GetPeriod((Timer *)this);
    this -> capture_count_1 = 0;
    this -> capture_count_2 = 0;
    this -> CAPTURE_DONE = 0;
    return this;
}


/*删除 Timer 实例*/
void TimerEp_Timer_Free(Timer * this) { free(this); }


/*---------- 定时器基本方法 -----------*/


/*设置定时器的cycle值*/
void TimerEp_SetCounter(Timer * this, int counter)
{
    __HAL_TIM_SET_COUNTER(this->htim, counter);
}


/*获取自动重装载寄存器值*/
int TimerEp_GetArr(Timer * this)
{
    return (int)( __HAL_TIM_GET_AUTORELOAD(this->htim) );
}


/*获取预分频寄存器值*/
int TimerEp_GetPsc(Timer * this)
{
    return (int)( this->htim->Instance->PSC );
}


/*获取计时器的完整计时周期（单位：ms）*/
float TimerEp_GetPeriod(Timer * this)
{
    float counts = (float)TimerEp_GetArr(this) + 1.0f;
    float divider = (float)TimerEp_GetPsc(this) + 1.0f;
    return counts * divider / (this->timer_clock_MHz * 1000.0f);
}


/*将毫秒转换为计数周期*/
float TimerEp_ms2cycle(Timer * this, float ms)
{
    return (float)( ms / TimerEp_GetPeriod(this) );
}


/*获取定时器的实例*/
TIM_TypeDef * TimerEp_GetInstance(Timer * this)
{
    return this->htim->Instance;
}


/*---------- 普通定时器 ----------*/


/*设置一个普通定时器的倒计时次数*/
void OrdinaryTimer_SetCycle(OrdinaryTimer * this, int cycle)
{
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();

    this->AC_flag = 0;
    this->cycle = cycle > 0 ? cycle : 0;

    __set_PRIMASK(saved_primask);
}


/*让一个普通定时器倒计时一次*/
void OrdinaryTimer_CountDown(OrdinaryTimer * this)
{
    if (this->cycle > 0) {
        this->cycle--;
        if (this->cycle == 0) this->AC_flag = 1;
    }
}


/*重置一个普通定时器*/
void OrdinaryTimer_Reset(OrdinaryTimer * this)
{
    OrdinaryTimer_SetCycle(this, 0);
}


/*检测一个普通定时器是否已经重置*/
int OrdinaryTimer_If_Reset(OrdinaryTimer * this)
{
    if (this->cycle == 0 && this->AC_flag == 0) {
        return 1;
    } else {
        return 0;
    }
}


/*检测一个普通定时器是否正在计数*/
int OrdinaryTimer_If_Counting(OrdinaryTimer * this)
{
    if (this->cycle > 0) {
        return 1;
    } else {
        return 0;
    }
}


/*检测一个普通定时器是否已经归零*/
int OrdinaryTimer_If_AC(OrdinaryTimer * this)
{
    if (this->cycle == 0 && this->AC_flag == 1) {
        return 1;
    } else {
        return 0;
    }
}


/*根据传入的时间（单位：ms）自动设置cycle值*/
void OrdinaryTimer_SetCycle_ms(OrdinaryTimer * this, float time_ms)
{
    OrdinaryTimer_SetCycle(this, (int)TimerEp_ms2cycle((Timer*)this, time_ms));
}


/*---------- PWM定时器 ----------*/


/*设置PWM定时器的compare值*/
void PwmTimer_SetCompare(PwmTimer * this, int compare)
{
    this->PWM_compare = compare;
    __HAL_TIM_SET_COMPARE(this->timer_base.htim, this->PWM_generation_channel, compare);
}


/*按比例设置PWM定时器的compare值（向上取整）*/
void PwmTimer_SetCompareRate(PwmTimer * this, float rate)
{
    if (rate < 0.0f || rate > 1.0f) return;
    int counts = TimerEp_GetArr((Timer *)this) + 1;
    int compare = (int)(rate * counts + 0.5f);
    PwmTimer_SetCompare(this, compare);
}


/*按时间设置PWM定时器的compare值*/
void PwmTimer_SetCompareTime(PwmTimer * this, float time)
{
    float rate = time / this->timer_base.period;
    if(rate > 1.0f) return;
    PwmTimer_SetCompareRate(this, rate);
}

/*获取PWM定时器的compare值*/
int PwmTimer_GetCompare(PwmTimer * this)
{
    return ( this->PWM_compare );
}


/*获取PWM定时器的有效电平比例*/
float PwmTimer_GetCompareRate(PwmTimer * this)
{
    int counts = TimerEp_GetArr((Timer *)this) + 1;
    return (float)PwmTimer_GetCompare(this) / counts;
}


/*获取PWM定时器的有效电平时间（单位：ms）*/
float PwmTimer_GetCompareTime(PwmTimer * this)
{
    return ( PwmTimer_GetCompareRate(this) * this->timer_base.period );
}


/*自动计算并返回一个PWM舵机的角度（将this用于驱动此舵机）（采用线性算法）*/
float PwmTimer_AutoServoAngle_Linear(   PwmTimer * this,
                                        float minDuty_percent, float maxDuty_percent,
                                        float minAngle_degree, float maxAngle_degree )
{
    float duty = PwmTimer_GetCompareRate(this);
    float minDuty = minDuty_percent / 100;
    float maxDuty = maxDuty_percent / 100;
    return ( minAngle_degree + (duty - minDuty) / (maxDuty - minDuty) * (maxAngle_degree - minAngle_degree) );
}


/*---------- 捕获定时器 ----------*/


/*拉高捕获定时器的触发器*/
void CaptureTimer_TriggerUp(CaptureTimer * this)
{
    HAL_GPIO_WritePin(this->trig_port, this->trig_pin, GPIO_PIN_SET);
}


/*拉低捕获定时器的触发器*/
void CaptureTimer_TriggerDown(CaptureTimer * this)
{
    HAL_GPIO_WritePin(this->trig_port, this->trig_pin, GPIO_PIN_RESET);
}


/*读取捕获定时器的第一通道的count值*/
volatile int CaptureTimer_ReadChannel_1(CaptureTimer * this)
{
    return ( HAL_TIM_ReadCapturedValue(this->timer_base.htim, this->capture_channel_1) );
}


/*读取捕获定时器的第二通道的count值*/
volatile int CaptureTimer_ReadChannel_2(CaptureTimer * this)
{
    return ( HAL_TIM_ReadCapturedValue(this->timer_base.htim, this->capture_channel_2) );
}


/*获取捕获定时器的 Active Channel*/
HAL_TIM_ActiveChannel CaptureTimer_GetActiveChannel(CaptureTimer * this)
{
    return ( this->capture_active_channel );
}


/*设置捕获定时器第一通道计数值*/
void CaptureTimer_AutoSetChannel_1(CaptureTimer * this)
{
    this->capture_count_1 = CaptureTimer_ReadChannel_1(this);
}


/*设置捕获定时器第二通道计数值*/
void CaptureTimer_AutoSetChannel_2(CaptureTimer * this)
{
    this->capture_count_2 = CaptureTimer_ReadChannel_2(this);
}


/*同时设置捕获定时器两个通道的计数值*/
void CaptureTimer_AutoSetChannel_both(CaptureTimer * this)
{
    CaptureTimer_AutoSetChannel_1(this);
    CaptureTimer_AutoSetChannel_2(this);
}


int CaptureTimer_GetCounter_Channel_1(CaptureTimer * this)
{
    return this->capture_count_1;
}


int CaptureTimer_GetCounter_Channel_2(CaptureTimer * this)
{
    return this->capture_count_2;
}