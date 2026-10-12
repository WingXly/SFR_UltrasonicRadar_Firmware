/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "flags.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "led.h"
#include "oled.h"
#include "font.h"
#include "timer_ep.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include "bluetooth.h"
#include "usart_ep.h"

#include "flags.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

// 传感器数据
float distance = 0;          // 距离（计算值）
float angle = 0;             // 角度（计算值）

/* USER CODE END Variables */
/* Definitions for MainTask */
osThreadId_t MainTaskHandle;
const osThreadAttr_t MainTask_attributes = {
  .name = "MainTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for OledTask */
osThreadId_t OledTaskHandle;
const osThreadAttr_t OledTask_attributes = {
  .name = "OledTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for BluetoothTask */
osThreadId_t BluetoothTaskHandle;
const osThreadAttr_t BluetoothTask_attributes = {
  .name = "BluetoothTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for sensor_data */
osMessageQueueId_t sensor_dataHandle;
const osMessageQueueAttr_t sensor_data_attributes = {
  .name = "sensor_data"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void *argument);
void StartOledTask(void *argument);
void StartBluetoothTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sensor_data */
  sensor_dataHandle = osMessageQueueNew (2, sizeof(float), &sensor_data_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of MainTask */
  MainTaskHandle = osThreadNew(StartMainTask, NULL, &MainTask_attributes);

  /* creation of OledTask */
  OledTaskHandle = osThreadNew(StartOledTask, NULL, &OledTask_attributes);

  /* creation of BluetoothTask */
  BluetoothTaskHandle = osThreadNew(StartBluetoothTask, NULL, &BluetoothTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartMainTask */
/**
  * @brief  Function implementing the MainTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartMainTask */
void StartMainTask(void *argument)
{
  /* USER CODE BEGIN StartMainTask */
  /* Infinite loop */
  for(;;)
  {
    // 遍历状态机
    switch (flags.state_machine)
    {
      case INIT:
        // 阻止HC-SR04在INIT状态下工作
        CaptureTimer_TriggerDown(capture_timer);

        // 启动时暂停
        LED_off(led_green);
        LED_on(led_red);
        flags.pause = true;

        // 启动时不启用OLED输出
        LED_off(led_blue);

        // 允许状态机步进
        OrdinaryTimer_Reset(main_cycle_timer);

        // 启动时将舵机置于0度
        PwmTimer_SetDuty(servo_pwm_timer, 0.025);

        // 启用蓝牙发送和接收
        Usart_mark_RxCplt(bluetooth, 1);
        Usart_mark_TxCplt(bluetooth, 1);

        // 等待命令
        Usart_ReceiveToIdle_DMA_noHT(bluetooth);

        osDelay(100);
        flags.state_machine = PAUSE_AND_EVENT_HANDLE;
        break;


      case PAUSE_AND_EVENT_HANDLE:
        // event: 按按钮（KEY1）暂停或解除暂停
        if (KEY1_IS_PRESSED())
        {
          osDelay(10);
          if (KEY1_IS_PRESSED()) {
            LED_toggle(led_green);
            LED_toggle(led_red);
            flags.pause = !flags.pause;
            
            while (KEY1_IS_PRESSED());
          }
        }

        // event: 更改compare step
        if (1 <= params.modify_step_optType && params.modify_step_optType <= 3)
        {
          AutoCompare_setStep(
            auto_compare,
            params.modify_step_optType,
            params.modify_step_value
          );
          params.modify_step_optType = 0;
        }

        //event: 更改cooling_time
        if (1 <= params.modify_cooling_time_optType && params.modify_cooling_time_optType <=3)
        {
          float cooling_time_old = flags.cooling_time_ms;
          
          if (params.modify_cooling_time_optType == 1) {        // set
              flags.cooling_time_ms = (float)params.modify_cooling_time_value / 1000;
          } else if (params.modify_cooling_time_optType == 2) { // add
              flags.cooling_time_ms += (float)params.modify_cooling_time_value / 1000;
          } else if (params.modify_cooling_time_optType == 3) { // remove
              flags.cooling_time_ms -= (float)params.modify_cooling_time_value / 1000;
          }

          // 不允许冷却时间小于1us
          if (flags.cooling_time_ms < 0.001) {
            flags.cooling_time_ms = cooling_time_old;
            send_error_bluetooth(0x00);
          }

          params.modify_cooling_time_optType = 0;
        }

        // 如果不处于暂停，则进行下一步
        if (flags.pause == false)
        {
          flags.state_machine = TRIG_UP_AND_DOWN;
        }
        break;


      case TRIG_UP_AND_DOWN:
        // 正在倒计时
        if (OrdinaryTimer_is_Counting(main_cycle_timer)) break;
        
        // 刚进入此状态，上拉Trig+开始倒计时
        if (OrdinaryTimer_is_Reset(main_cycle_timer))
        {
          CaptureTimer_mark_CaptureCplt(capture_timer, 0);
          CaptureTimer_TriggerUp(capture_timer);
          OrdinaryTimer_SetCycle_ms(main_cycle_timer, TRIG_DURATION_ms);
        }

        // 倒计时完毕，将Trig下拉，开始捕获
        if (OrdinaryTimer_is_CountCplt(main_cycle_timer))
        {
          TimerEp_SetCounter((Timer *)capture_timer, 0); // 将TIM1清零，防止测量时自动重装载
          CaptureTimer_TriggerDown(capture_timer); // 下拉既代表开始捕获
          OrdinaryTimer_Reset(main_cycle_timer); // 重置计时器
          flags.state_machine = MEASURING;
        }
        break;


      case MEASURING:
        // 如果捕获完毕
        if (CaptureTimer_is_CaptureCplt(capture_timer)) {
          flags.state_machine = PROCESSING;
        }
        break;


      case PROCESSING:
        distance = calc_distance_mm(
          CaptureTimer_LoadCounter_1(capture_timer),
          CaptureTimer_LoadCounter_2(capture_timer)
        );
        angle = PwmTimer_GetCompare_AsServoAngle(
          servo_pwm_timer,
          PWMSERVO_MIN_COMPARE,
          PWMSERVO_MAX_COMPARE,
          PWMSERVO_ANGLE_RANGE
        );
        flags.state_machine = ROTATE;
        break;


      case ROTATE:
        PwmTimer_SetCompare(
          servo_pwm_timer,
          AutoCompare_next(auto_compare)
        );
        OrdinaryTimer_Reset(main_cycle_timer);
        flags.oled_refresh_req = true;
        osMessageQueuePut(sensor_dataHandle, &distance, 0, 0);
        osMessageQueuePut(sensor_dataHandle, &angle, 0, 0);
        flags.state_machine = COOLING;
        break;


      case COOLING:
        // 正在倒计时
        if (OrdinaryTimer_is_Counting(main_cycle_timer)) break;

        // 刚进入COOLING，开始倒计时
        if (OrdinaryTimer_is_Reset(main_cycle_timer))
        {
          OrdinaryTimer_SetCycle_ms(
            main_cycle_timer,
            flags.cooling_time_ms
          );
        }

        // 倒计时完毕
        if (OrdinaryTimer_is_CountCplt(main_cycle_timer))
        {
          OrdinaryTimer_Reset(main_cycle_timer);
          flags.state_machine = PAUSE_AND_EVENT_HANDLE;
        }
        break;
      }

      osDelay(1);
  }

  /* USER CODE END StartMainTask */
}

/* USER CODE BEGIN Header_StartOledTask */
/**
* @brief Function implementing the OledTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartOledTask */
void StartOledTask(void *argument)
{
  /* USER CODE BEGIN StartOledTask */
  /* Infinite loop */
  for(;;)
  {
    // event: 按按钮（KEY2）启用或禁用OLED输出
    if (KEY2_IS_PRESSED())
    {
      HAL_Delay(10);
      if (KEY2_IS_PRESSED()) {
        LED_toggle(led_blue);
        flags.enable_oled = !flags.enable_oled;

        if (flags.enable_oled) flags.oled_refresh_req = true;
        else flags.oled_clear_req = true;
            
        while (KEY2_IS_PRESSED());
      }
    }

    // event: 清空或显示OLED
    if (flags.oled_clear_req)
    {
      flags.oled_clear_req = false;
      OLED_NewFrame();
      OLED_ShowFrame();
    }
    else if (flags.oled_refresh_req && flags.enable_oled)
    {
      flags.oled_refresh_req = false;
      print_sensor_data_on_oled(distance, angle);
    }

    osDelay(1);
  }
  /* USER CODE END StartOledTask */
}

/* USER CODE BEGIN Header_StartBluetoothTask */
/**
* @brief Function implementing the BluetoothTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBluetoothTask */
void StartBluetoothTask(void *argument)
{
  /* USER CODE BEGIN StartBluetoothTask */
  /* Infinite loop */
  for(;;)
  {
    // event: 蓝牙接收完毕
    if (Usart_is_RxCplt(bluetooth))
    {
      Usart_mark_RxCplt(bluetooth, 0);
      bluetooth_cmd_handler(bluetooth);
      Usart_ReceiveToIdle_DMA_noHT(bluetooth);
    }

    // event: 蓝牙发送完毕
    if (Usart_is_TxCplt(bluetooth)
        && osMessageQueueGetCount(sensor_dataHandle) == 2 )
    {
      Usart_mark_TxCplt(bluetooth, 0);
      float d = 0;
      float a = 0;
      osMessageQueueGet(sensor_dataHandle, &d, 0, 0);
      osMessageQueueGet(sensor_dataHandle, &a, 0, 0);
      send_sensor_data_bluetooth(d, a);
    }

    osDelay(1);
  }
  /* USER CODE END StartBluetoothTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

