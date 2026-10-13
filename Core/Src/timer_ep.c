/**
 * @file timer_ep.c
 * @brief 定时器的面向对象封装
 * @anchor WingXly
 * @version 1.0
 *
 * @attention
 * license MIT
 *
 * @note
 * 本文件按用途提供了三种不同类型的定时器：
 * 1. 普通定时器（OrdinaryTimer），用于倒计时
 * 2. PWM定时器（PwmTimer），用于控制一个Channel的compare输出
 * 3. 捕获定时器（CaptureTimer）
 *
 * @note
 * 使用流程:
 * 1. 在任意位置声明一个定时器，例如：`OrdinaryTimer * my_ord_timer = NULL;`
 * 2. 在main()或其他函数中实例化，例如：`my_ord_timer = OrdinaryTimer_Instantiate(&htim1, 72);`
 * 3. 在实例化之后使用函数控制定时器，具体详见本文代码
 * 4. 当无需使用一个实例时，将它销毁：`TimerEp_Free((Timer*)my_ord_timer);`
 *
 * @note
 * 为保持封装，务必要在一个函数中实例化，而不是在将对象声明为全局变量时直接实例化。
 *
 */
#include "timer_ep.h"
#include "stm32f1xx_hal_tim.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>


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
    volatile int compare_multiplier;
    volatile int compare_step;
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
                                int PWM_generation_channel,
                                int compare_step )
{
    PwmTimer * this = malloc(sizeof(PwmTimer));
    if (!this) return NULL;
    this -> timer_base.htim = htim;
    this -> timer_base.timer_clock_MHz = timer_clock_MHz;
    this -> PWM_compare = 0;
    this -> PWM_generation_channel = PWM_generation_channel;
    this -> timer_base.period = TimerEp_GetPeriod((Timer *)this);
    this -> compare_multiplier = 1;
    this -> compare_step = compare_step;
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


/*设置定时器的计数值*/
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


/*启动定时器（不启用中断）*/
void OrdinaryTimer_Init(OrdinaryTimer * this)
{
    HAL_TIM_Base_Start(this->timer_base.htim);
}

/*启动定时器（启用中断）*/
void OrdinaryTimer_Init_IT(OrdinaryTimer * this)
{
    HAL_TIM_Base_Start_IT(this->timer_base.htim);
}


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
        if (this->cycle == 0) {
            this->AC_flag = 1;
        }
    }
}


/*重置一个普通定时器*/
void OrdinaryTimer_Reset(OrdinaryTimer * this)
{
    OrdinaryTimer_SetCycle(this, 0);
}


/*检测一个普通定时器是否已经重置*/
int OrdinaryTimer_is_Reset(OrdinaryTimer * this)
{
    if (this->cycle == 0 && this->AC_flag == 0) {
        return 1;
    } else {
        return 0;
    }
}


/*检测一个普通定时器是否正在倒计时*/
int OrdinaryTimer_is_Counting(OrdinaryTimer * this)
{
    if (this->cycle > 0) {
        return 1;
    } else {
        return 0;
    }
}


/*检测一个普通定时器是否倒计时完毕*/
int OrdinaryTimer_is_CountCplt(OrdinaryTimer * this)
{
    if (this->cycle == 0 && this->AC_flag == 1) {
        return 1;
    } else {
        return 0;
    }
}


/*将传入的时间（单位：ms）设置为普通计时器的cycle值*/
void OrdinaryTimer_SetCycle_ms(OrdinaryTimer * this, float time_ms)
{
    OrdinaryTimer_SetCycle(this, (int)TimerEp_ms2cycle((Timer*)this, time_ms));
}


/*---------- PWM定时器 ----------*/


/*启动PWM定时器*/
void PwmTimer_Init(PwmTimer * this)
{
    HAL_TIM_PWM_Start(this->timer_base.htim, this->PWM_generation_channel);
}


/*设置PWM定时器的compare值*/
void PwmTimer_SetCompare(PwmTimer * this, int compare)
{
    this->PWM_compare = compare;
    __HAL_TIM_SET_COMPARE(this->timer_base.htim, this->PWM_generation_channel, compare);
}


/*按比例设置PWM定时器的compare值（向上取整）*/
void PwmTimer_SetDuty(PwmTimer * this, float rate)
{
    if (rate < 0.0f || rate > 1.0f) return;
    int counts = TimerEp_GetArr((Timer *)this) + 1;
    int compare = (int)(rate * counts + 0.5f);
    PwmTimer_SetCompare(this, compare);
}


/*按时间设置PWM定时器的compare值*/
void PwmTimer_SetDuration(PwmTimer * this, float time)
{
    float rate = time / this->timer_base.period;
    if(rate > 1.0f) return;
    PwmTimer_SetDuty(this, rate);
}


/*获取PWM定时器的compare值*/
int PwmTimer_GetCompare(PwmTimer * this)
{
    return ( this->PWM_compare );
}


/*获取PWM定时器的有效电平比例*/
float PwmTimer_GetDuty(PwmTimer * this)
{
    int counts = TimerEp_GetArr((Timer *)this) + 1;
    return (float)PwmTimer_GetCompare(this) / counts;
}


/*获取PWM定时器的有效电平时间（单位：ms）*/
float PwmTimer_GetDuration(PwmTimer * this)
{
    return ( PwmTimer_GetDuty(this) * this->timer_base.period );
}


/*使用compare值计算并返回一个PWM舵机的角度（将this用于驱动此舵机）*/
float PwmTimer_GetCompare_AsServoAngle( PwmTimer * this,
                                        float minCompare, float maxCompare,
                                        float angleRange_degree )
{
    int compare = PwmTimer_GetCompare(this);
    return (compare - minCompare) / (maxCompare - minCompare) * (angleRange_degree);
}


/*使用占空比计算并返回一个PWM舵机的角度（将this用于驱动此舵机）*/
float PwmTimer_GetDuty_AsServoAngle( PwmTimer * this,
                                        float minDuty_percent, float maxDuty_percent,
                                        float angleRange_degree )
{
    float duty = PwmTimer_GetDuty(this);
    float minDuty = minDuty_percent / 100;
    float maxDuty = maxDuty_percent / 100;
    return (duty - minDuty) / (maxDuty - minDuty) * (angleRange_degree);
}


/*PWM舵机的线性控制算法。自动计算TIM4的下一个compare值并返回它*/
int PwmTimer_NextCompare_Linear(PwmTimer * this,
                                float minDuty_percent, float maxDuty_percent )
{
  int Compare = PwmTimer_GetCompare(this);
  // 钳制Compare，同时计算其乘数的值
  int ARR = TimerEp_GetArr((Timer*)this);
  int Compare_max = (int)(ARR * maxDuty_percent/100 + 0.5f); // 向上取整+0.5
  int Compare_min = (int)(ARR * minDuty_percent/100 + 0.5f);

  if (Compare >= Compare_max) {
    Compare = Compare_max;
    this -> compare_multiplier = -1;
  }
  else if (Compare <= Compare_min) {
    Compare = Compare_min;
    this -> compare_multiplier = 1;
  }

  return (Compare + this->compare_multiplier * this->compare_step);
}


/*自动设置PWM定时器的compare值（线性算法）*/
void PwmTimer_SetCompare_Auto_Linear_ForthAndBack(PwmTimer * this,
                                    float minDuty_percent, float maxDuty_percent )
{
    PwmTimer_SetCompare(
        this,
        PwmTimer_NextCompare_Linear(this, minDuty_percent, maxDuty_percent)
    );
}


/*---------- 捕获定时器 ----------*/


/*启动捕获定时器*/
void CaptureTimer_Init(CaptureTimer * this)
{
    HAL_TIM_Base_Start(this->timer_base.htim);
    HAL_TIM_IC_Start(this->timer_base.htim, this->capture_channel_1);
    HAL_TIM_IC_Start_IT(this->timer_base.htim, this->capture_channel_2);
}


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
volatile int CaptureTimer_ReadCounter_1(CaptureTimer * this)
{
    return ( HAL_TIM_ReadCapturedValue(this->timer_base.htim, this->capture_channel_1) );
}


/*读取捕获定时器的第二通道的count值*/
volatile int CaptureTimer_ReadCounter_2(CaptureTimer * this)
{
    return ( HAL_TIM_ReadCapturedValue(this->timer_base.htim, this->capture_channel_2) );
}


/*获取捕获定时器的 Active Channel*/
HAL_TIM_ActiveChannel CaptureTimer_GetActiveChannel(CaptureTimer * this)
{
    return ( this->capture_active_channel );
}


/*读取并保存捕获定时器第一通道计数值*/
void CaptureTimer_SaveCounter_Auto_1(CaptureTimer * this)
{
    this->capture_count_1 = CaptureTimer_ReadCounter_1(this);
}


/*读取并保存捕获定时器第二通道计数值*/
void CaptureTimer_SaveCounter_Auto_2(CaptureTimer * this)
{
    this->capture_count_2 = CaptureTimer_ReadCounter_2(this);
}


/*同时保存捕获定时器两个通道的计数值*/
void CaptureTimer_SaveCounter_both(CaptureTimer * this)
{
    CaptureTimer_SaveCounter_Auto_1(this);
    CaptureTimer_SaveCounter_Auto_2(this);
    CaptureTimer_mark_CaptureCplt(this, 1);
}


/*加载已保存的捕获定时器第一通道计数值*/
int CaptureTimer_LoadCounter_1(CaptureTimer * this)
{
    return this->capture_count_1;
}


/*加载已保存的捕获定时器第二通道计数值*/
int CaptureTimer_LoadCounter_2(CaptureTimer * this)
{
    return this->capture_count_2;
}


/*将一个捕获定时器的状态设置为已完成捕获或未完成捕获*/
void CaptureTimer_mark_CaptureCplt(CaptureTimer * this, int is_it_complete)
{
    this->CAPTURE_DONE = is_it_complete;
}


/*读取一个捕获定时器的状态（已完成捕获或未完成捕获）*/
int CaptureTimer_is_CaptureCplt(CaptureTimer * this)
{
    return this->CAPTURE_DONE;
}