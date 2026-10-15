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
/* Definitions for Task_Main */
osThreadId_t Task_MainHandle;
const osThreadAttr_t Task_Main_attributes = {
  .name = "Task_Main",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for Task_OledUpdate */
osThreadId_t Task_OledUpdateHandle;
const osThreadAttr_t Task_OledUpdate_attributes = {
  .name = "Task_OledUpdate",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for Task_BluetoothIO */
osThreadId_t Task_BluetoothIOHandle;
const osThreadAttr_t Task_BluetoothIO_attributes = {
  .name = "Task_BluetoothIO",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for Task_Measure */
osThreadId_t Task_MeasureHandle;
const osThreadAttr_t Task_Measure_attributes = {
  .name = "Task_Measure",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal1,
};
/* Definitions for queue_btSendData_ */
osMessageQueueId_t queue_btSendData_Handle;
const osMessageQueueAttr_t queue_btSendData__attributes = {
  .name = "queue_btSendData_"
};
/* Definitions for queue_btRxCommand_ */
osMessageQueueId_t queue_btRxCommand_Handle;
const osMessageQueueAttr_t queue_btRxCommand__attributes = {
  .name = "queue_btRxCommand_"
};
/* Definitions for binSignal_awake_BtTask_ */
osSemaphoreId_t binSignal_awake_BtTask_Handle;
const osSemaphoreAttr_t binSignal_awake_BtTask__attributes = {
  .name = "binSignal_awake_BtTask_"
};
/* Definitions for binSignal_awake_OledUpdate_ */
osSemaphoreId_t binSignal_awake_OledUpdate_Handle;
const osSemaphoreAttr_t binSignal_awake_OledUpdate__attributes = {
  .name = "binSignal_awake_OledUpdate_"
};
/* Definitions for binSignal_awake_MainCycle_ */
osSemaphoreId_t binSignal_awake_MainCycle_Handle;
const osSemaphoreAttr_t binSignal_awake_MainCycle__attributes = {
  .name = "binSignal_awake_MainCycle_"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */




void awake_bt_task()
{
  osSemaphoreRelease(binSignal_awake_BtTask_Handle);
}


void put_queue_btSendData_awake(float data1, float data2)
{
  osMessageQueuePut(queue_btSendData_Handle, &data1, 0, 0);
  osMessageQueuePut(queue_btSendData_Handle, &data2, 0, 0);
  awake_bt_task();
}


void put_queue_btSendError_awake(uint16_t error_code)
{
    put_queue_btSendData_awake(-1.0f, error_code);
}


void awake_oled_update(bool oled_is_show)
{
  oled_refresh_req = oled_is_show;
  oled_clear_req = !oled_is_show;
  osSemaphoreRelease(binSignal_awake_OledUpdate_Handle);
}


void awake_main_task()
{
  osSemaphoreRelease(binSignal_awake_MainCycle_Handle);
}


void queue_btRxCommand_put(uint16_t * p)
{
  osMessageQueuePut(queue_btRxCommand_Handle, p, 0, 0);
}







/* USER CODE END FunctionPrototypes */

void StartTask_Main(void *argument);
void StartTask_OledUpdate(void *argument);
void StartTask_BluetoothIO(void *argument);
void StartTask_Measure(void *argument);

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
  /* creation of binSignal_awake_BtTask_ */
  binSignal_awake_BtTask_Handle = osSemaphoreNew(1, 0, &binSignal_awake_BtTask__attributes);

  /* creation of binSignal_awake_OledUpdate_ */
  binSignal_awake_OledUpdate_Handle = osSemaphoreNew(1, 0, &binSignal_awake_OledUpdate__attributes);

  /* creation of binSignal_awake_MainCycle_ */
  binSignal_awake_MainCycle_Handle = osSemaphoreNew(1, 0, &binSignal_awake_MainCycle__attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of queue_btSendData_ */
  queue_btSendData_Handle = osMessageQueueNew (16, sizeof(float), &queue_btSendData__attributes);

  /* creation of queue_btRxCommand_ */
  queue_btRxCommand_Handle = osMessageQueueNew (16, sizeof(uint16_t), &queue_btRxCommand__attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Task_Main */
  Task_MainHandle = osThreadNew(StartTask_Main, NULL, &Task_Main_attributes);

  /* creation of Task_OledUpdate */
  Task_OledUpdateHandle = osThreadNew(StartTask_OledUpdate, NULL, &Task_OledUpdate_attributes);

  /* creation of Task_BluetoothIO */
  Task_BluetoothIOHandle = osThreadNew(StartTask_BluetoothIO, NULL, &Task_BluetoothIO_attributes);

  /* creation of Task_Measure */
  Task_MeasureHandle = osThreadNew(StartTask_Measure, NULL, &Task_Measure_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}




/* USER CODE BEGIN Header_StartTask_Main */
/**
  * @brief  Function implementing the Task_Main thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTask_Main */
void StartTask_Main(void *argument)
{
  /* USER CODE BEGIN StartTask_Main */
  /* Infinite loop */
  for(;;)
  {
// 遍历状态机
switch (state_machine) {
case INIT:
        // 阻止HC-SR04在INIT状态下工作
        CaptureTimer_TriggerDown(capture_timer);

        // 启动时暂停
        LED_off(led_green);
        LED_on(led_red);
        pause = true;

        // 启动时不启用OLED输出
        oled_refresh_req = 0;
        LED_off(led_blue);

        // 重置计时器
        OrdinaryTimer_Reset(main_cycle_timer);

        // 启动时将舵机置于0度
        PwmTimer_SetCompare(servo_pwm_timer, PWMSERVO_MIN_COMPARE);

        // 启用蓝牙发送和接收
        Usart_mark_RxCplt(bluetooth, 1);
        Usart_mark_TxCplt(bluetooth, 1);

        // 等待接收命令
        Usart_ReceiveToIdle_DMA_noHT(bluetooth);

        osDelay(100);
        state_machine = PAUSE_AND_EVENT_HANDLE;
      break;


case PAUSE_AND_EVENT_HANDLE:
        // 指令处理
        uint8_t cmd_type = 0;
        uint8_t para1 = 0;
        osMessageQueueGet(queue_btRxCommand_Handle, &cmd_type, 0, 0);
        osMessageQueueGet(queue_btRxCommand_Handle, &para1, 0, 0);
          
        if (osMessageQueueGetCount(queue_btRxCommand_Handle) == 0) {
          command_handler_1para(cmd_type, para1);
        } else {
          uint8_t para2 = 0;
          osMessageQueueGet(queue_btRxCommand_Handle, &para2, 0, 0);
          command_handler_2para(cmd_type, para1, para2);
        }
        // 按按钮（KEY1）暂停或解除暂停的事件放在main.c的HAL_GPIO_EXTI_Callback()里
        // 如果不处于暂停，则进行下一步
        if (!pause) {
          state_machine = MEASURING;
        } else
      break;


case MEASURING:
        // 刚进入此状态，上拉Trig+开始倒计时
        CaptureTimer_TriggerUp(capture_timer);
        OrdinaryTimer_SetCycle_ms(main_cycle_timer, TRIG_DURATION_ms);

        // 等待倒计时结束，将Trig下拉，开始捕获
        osSemaphoreAcquire(binSignal_awake_MainCycle_Handle, osWaitForever);
        TimerEp_SetCounter((Timer *)capture_timer, 0);
        CaptureTimer_TriggerDown(capture_timer);
        OrdinaryTimer_Reset(main_cycle_timer);

        // 等待捕获结束
        osSemaphoreAcquire(binSignal_awake_MainCycle_Handle, osWaitForever);
        state_machine = PROCESSING;


case PROCESSING:
        // 计算传感器数据
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


case ROTATE:
        if (resample_remain) {
          resample_remain --;
        } else {
          resample_remain = resample;
          PwmTimer_SetCompare(
            servo_pwm_timer,
            AutoCompare_next(auto_compare)
          );
        }
        
        state_machine = COOLING;


case COOLING:
        // 刚进入COOLING，开始倒计时，刷新oled，发送传感器数据
        OrdinaryTimer_SetCycle_ms(
          main_cycle_timer,
          cooling_time_ms
        );
        if(enable_oled) awake_oled_update(true);
        put_queue_btSendData_awake(distance, angle);

        // 等待倒计时结束
        osSemaphoreAcquire(binSignal_awake_MainCycle_Handle, osWaitForever);

        // 倒计时完毕
        state_machine = PAUSE_AND_EVENT_HANDLE;
}

      osDelay(1);
  }

  /* USER CODE END StartTask_Main */
}




/* USER CODE BEGIN Header_StartTask_OledUpdate */
/**
* @brief Function implementing the Task_OledUpdate thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_OledUpdate */
void StartTask_OledUpdate(void *argument)
{
  /* USER CODE BEGIN StartTask_OledUpdate */
  /* Infinite loop */
  for(;;)
  {
    // 等待信号量释放
    osSemaphoreAcquire(binSignal_awake_OledUpdate_Handle, osWaitForever);

    // 有关按按钮（KEY2）的处理放在main.c的HAL_GPIO_EXTI_Callback()里
    
    // 清空或显示OLED
    if (oled_clear_req) {
      oled_clear_req = false;
      OLED_NewFrame();
      OLED_ShowFrame();
    }
    else if (oled_refresh_req && enable_oled) {
      oled_refresh_req = false;
      print_sensor_data_on_oled(distance, angle);
    }

    osDelay(1);
  }
  /* USER CODE END StartTask_OledUpdate */
}




/* USER CODE BEGIN Header_StartTask_BluetoothIO */
/**
* @brief Function implementing the Task_BluetoothIO thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_BluetoothIO */
void StartTask_BluetoothIO(void *argument)
{
  /* USER CODE BEGIN StartTask_BluetoothIO */
  /* Infinite loop */
  for(;;)
  {
    // 等待信号量释放
    osSemaphoreAcquire(binSignal_awake_BtTask_Handle, osWaitForever);
    
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
    if (osMessageQueueGetCount(queue_btSendData_Handle) >= 2)
    {
      while(!Usart_is_TxCplt(bluetooth));
      // 获取将要发送的数据
      static float sendData_1 = 0;
      static float sendData_2 = 0;
      osMessageQueueGet(queue_btSendData_Handle, &sendData_1, 0, 0);
      osMessageQueueGet(queue_btSendData_Handle, &sendData_2, 0, 0);

      // 判断发送的数据是传感器数据还是错误信息，然后发送
      if (sendData_1 == -1.0f) {
        if (sendData_2 < 0) sendData_2 = 65535;
        send_error(bluetooth, (uint16_t)sendData_2);
      } else {
        send_sensor_data(bluetooth, sendData_1, sendData_2);
      }

      // 如果队列里还有数据，就再让蓝牙发送一次（不阻塞其他任务）
      if (osMessageQueueGetCount(queue_btSendData_Handle) >= 2 ) {
        osSemaphoreRelease(binSignal_awake_BtTask_Handle);
      }

      // 清理垃圾数据
      else if (osMessageQueueGetCount(queue_btSendData_Handle) == 1) {
        osMessageQueueGet(queue_btSendData_Handle, &sendData_1, 0, 0);
      }
    }

    osDelay(1);
  }
  /* USER CODE END StartTask_BluetoothIO */
}




/* USER CODE BEGIN Header_StartTask_Measure */
/**
* @brief Function implementing the Task_Measure thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_Measure */
void StartTask_Measure(void *argument)
{
  /* USER CODE BEGIN StartTask_Measure */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask_Measure */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

