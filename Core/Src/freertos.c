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
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "led.h"
#include "oled.h"
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
/* Definitions for bt_send_data */
osMessageQueueId_t bt_send_dataHandle;
const osMessageQueueAttr_t bt_send_data_attributes = {
  .name = "bt_send_data"
};
/* Definitions for BtTaskWaker */
osSemaphoreId_t BtTaskWakerHandle;
const osSemaphoreAttr_t BtTaskWaker_attributes = {
  .name = "BtTaskWaker"
};
/* Definitions for OledUpdateWaker */
osSemaphoreId_t OledUpdateWakerHandle;
const osSemaphoreAttr_t OledUpdateWaker_attributes = {
  .name = "OledUpdateWaker"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */


void awake_bt_send_data(float data1, float data2)
{
  osMessageQueuePut(bt_send_dataHandle, &data1, 0, 0);
  osMessageQueuePut(bt_send_dataHandle, &data2, 0, 0);
  osSemaphoreRelease(bt_send_dataHandle);
}


void awake_bt_send_error(uint16_t error_code)
{
    awake_bt_send_data(-1.0f, error_code);
}


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

  /* Create the semaphores(s) */
  /* creation of BtTaskWaker */
  BtTaskWakerHandle = osSemaphoreNew(1, 0, &BtTaskWaker_attributes);

  /* creation of OledUpdateWaker */
  OledUpdateWakerHandle = osSemaphoreNew(1, 0, &OledUpdateWaker_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of bt_send_data */
  bt_send_dataHandle = osMessageQueueNew (8, sizeof(float), &bt_send_data_attributes);

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
    switch (state_machine)
    {
      case INIT:
        // 阻止HC-SR04在INIT状态下工作
        CaptureTimer_TriggerDown(capture_timer);

        // 启动时暂停
        LED_off(led_green);
        LED_on(led_red);
        pause = true;

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
        state_machine = PAUSE_AND_EVENT_HANDLE;
        break;


      case PAUSE_AND_EVENT_HANDLE:
        // event: 按按钮（KEY1）暂停或解除暂停
        if (KEY1_IS_PRESSED())
        {
          osDelay(10);
          if (KEY1_IS_PRESSED()) {
            LED_toggle(led_green);
            LED_toggle(led_red);
            pause = !pause;
            
            while (KEY1_IS_PRESSED());
          }
        }

        // event: 更改compare step
        if (1 <= modify_step_optType && modify_step_optType <= 3)
        {
          AutoCompare_setStep(
            auto_compare,
            modify_step_optType,
            modify_step_value
          );
          modify_step_optType = 0;
        }

        //event: 更改cooling_time
        if (1 <= modify_cooling_time_optType && modify_cooling_time_optType <=3)
        {
          float cooling_time_old = cooling_time_ms;
          
          if (modify_cooling_time_optType == 1) {        // set
              cooling_time_ms = (float)modify_cooling_time_value / 1000;
          } else if (modify_cooling_time_optType == 2) { // add
              cooling_time_ms += (float)modify_cooling_time_value / 1000;
          } else if (modify_cooling_time_optType == 3) { // remove
              cooling_time_ms -= (float)modify_cooling_time_value / 1000;
          }

          // 不允许冷却时间小于1us
          if (cooling_time_ms < 0.001) {
            cooling_time_ms = cooling_time_old;
            awake_bt_send_error(0x00);
          }

          modify_cooling_time_optType = 0;
        }

        // 如果不处于暂停，则进行下一步
        if (pause == false)
        {
          state_machine = TRIG_UP_AND_DOWN;
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
          state_machine = MEASURING;
        }
        break;


      case MEASURING:
        // 如果捕获完毕
        if (CaptureTimer_is_CaptureCplt(capture_timer)) {
          state_machine = PROCESSING;
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
        state_machine = ROTATE;
        break;


      case ROTATE:
        PwmTimer_SetCompare(
          servo_pwm_timer,
          AutoCompare_next(auto_compare)
        );
        OrdinaryTimer_Reset(main_cycle_timer);
        oled_refresh_req = true;
        awake_bt_send_data(distance, angle);
        state_machine = COOLING;
        break;


      case COOLING:
        // 正在倒计时
        if (OrdinaryTimer_is_Counting(main_cycle_timer)) break;

        // 刚进入COOLING，开始倒计时
        if (OrdinaryTimer_is_Reset(main_cycle_timer))
        {
          OrdinaryTimer_SetCycle_ms(
            main_cycle_timer,
            cooling_time_ms
          );
        }

        // 倒计时完毕
        if (OrdinaryTimer_is_CountCplt(main_cycle_timer))
        {
          OrdinaryTimer_Reset(main_cycle_timer);
          state_machine = PAUSE_AND_EVENT_HANDLE;
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
    // 等待信号量释放
    osSemaphoreAcquire(OledUpdateWakerHandle, osWaitForever);

    // 有关按按钮（KEY2）的处理放在main.c的HAL_GPIO_EXTI_Callback()里
    
    // 清空或显示OLED
    if (oled_clear_req) {
      oled_clear_req = false;
      OLED_NewFrame();
      OLED_ShowFrame();
    } else if (oled_refresh_req && enable_oled) {
      oled_refresh_req = false;
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
    // 等待信号量释放
    osSemaphoreAcquire(BtTaskWakerHandle, osWaitForever);
    
    // event: 蓝牙接收完毕
    if (Usart_is_RxCplt(bluetooth))
    {
      // 接收指令，然后执行
      cmd_handler(bluetooth);

      // 等待下一个命令
      Usart_mark_RxCplt(bluetooth, 0);
      Usart_ReceiveToIdle_DMA_noHT(bluetooth);
    }

    // event: 如果队列中有需要发送的东西，就发送它们
    if (osMessageQueueGetCount(bt_send_dataHandle) >= 2 )
    {
      // 获取将要发送的数据
      float sendData_1 = 0;
      float sendData_2 = 0;
      osMessageQueueGet(bt_send_dataHandle, &sendData_1, 0, 0);
      osMessageQueueGet(bt_send_dataHandle, &sendData_2, 0, 0);

      // 判断发送的数据是传感器数据还是错误信息，然后发送
      if (sendData_1 == -1.0f) {
        if (sendData_2 < 0) sendData_2 = 1000;
        send_error(bluetooth, (uint16_t)sendData_2);
      } else {
        send_sensor_data(bluetooth, sendData_1, sendData_2);
      }

      // 如果队列里还有数据，就再让蓝牙发送一次（不阻塞其他任务）
      if (osMessageQueueGetCount(bt_send_dataHandle) >= 2 )
        osSemaphoreRelease(BtTaskWakerHandle);
    }

    osDelay(1);
  }
  /* USER CODE END StartBluetoothTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

