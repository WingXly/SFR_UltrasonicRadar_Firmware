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
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "led.h"
#include "oled.h"
#include "font.h"
#include "timer_ep.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include "bluetooth.h"
#include "usart_ep.h"
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

// 全局标志位
volatile StateMachine_TypeDef StateMachine = INIT; // 状态机

volatile int PAUSE = 0;                            // 是否暂停（IDLE默认暂停）
volatile int ENABLE_OLED = 0;                      // 是否启用OLED输出（IDLE默认不启用）

volatile int OLED_REFRESH_REQUEST = 0;             // 请求OLED屏幕刷新标志位
volatile int OLED_CLEAR_REQUEST = 0;               // 请求OLED屏幕清空标志位

volatile int modify_step_optType = 0;             // 修改step值的操作类型（1:set 2:add 3:remove）
volatile int modify_step_value = 1;                // 修改step值的具体数字

volatile int modify_cooling_time_optType = 0;     // 修改冷却时间的操作类型（1:set 2:add 3:remove）
volatile int modify_cooling_time_value = 0;        // 修改冷却时间的具体数数字

volatile float cooling_time_ms = 30.0f;            // 冷却时间

// 蓝牙串口
Usart * bluetooth = NULL;

// LED小灯
LED * led_red   = NULL;
LED * led_green = NULL;
LED * led_blue  = NULL;

// 定时器
CaptureTimer  * capture_timer   = NULL;
OrdinaryTimer * main_cycle_timer = NULL;
PwmTimer      * servo_pwm_timer  = NULL;

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

  // 实例化LED小灯
  led_red = LED_Instantiate(LED_RED_GPIO_Port, LED_RED_Pin);
  led_green = LED_Instantiate(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  led_blue = LED_Instantiate(LED_BLUE_GPIO_Port, LED_BLUE_Pin);


  // 实例化定时器
  capture_timer = CaptureTimer_Instantiate(
    &htim1,
    72,
    HC_SR04_Trig_GPIO_Port,
    HC_SR04_Trig_Pin,
    TIM_CHANNEL_3,
    TIM_CHANNEL_4,
    HAL_TIM_ACTIVE_CHANNEL_4
  );

  main_cycle_timer = OrdinaryTimer_Instantiate(
    &htim3,
    72
  );

  servo_pwm_timer = PwmTimer_Instantiate(
    &htim4,
    72,
    TIM_CHANNEL_3,
    1
  );


  // 实例化蓝牙串口
  extern DMA_HandleTypeDef hdma_usart3_rx;
  extern DMA_HandleTypeDef hdma_usart3_tx;
  bluetooth = Usart_Instantiate(
    &huart3,
    &hdma_usart3_rx,
    &hdma_usart3_tx
  );


  // 实例化AutoCompare
  AutoCompare_TypeDef ac_ = {
    .compare     = PWMSERVO_MIN_COMPARE,
    .multiplier  = 1,
    .step        = 1,
    .compare_min = PWMSERVO_MIN_COMPARE,
    .compare_max = PWMSERVO_MAX_COMPARE
  };
  AutoCompare_TypeDef * auto_compare = &ac_;


  // 启动外设
  osDelay(20);

  CaptureTimer_Init(capture_timer);
  OrdinaryTimer_Init(main_cycle_timer);
  PwmTimer_Init(servo_pwm_timer);

  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, "Start", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();

  
  /* Infinite loop */

  for(;;)
  {
    // 遍历状态机
    switch (StateMachine)
    {
      case INIT:
        // 阻止HC-SR04在INIT状态下工作
        CaptureTimer_TriggerDown(capture_timer);

        // 启动时暂停
        LED_off(led_green);
        LED_on(led_red);
        PAUSE = 1;

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
        StateMachine = PAUSE_AND_EVENT_HANDLE;
        break;


      case PAUSE_AND_EVENT_HANDLE:
        // event: 按按钮（KEY1）暂停或解除暂停
        if (KEY1_IS_PRESSED()) {
          osDelay(10);
          if (KEY1_IS_PRESSED()) {
            LED_toggle(led_green);
            LED_toggle(led_red);
            PAUSE = 1 - PAUSE;
            
            while (KEY1_IS_PRESSED());
          }
        }

        // event: 更改compare step
        if (1 <= modify_step_optType && modify_step_optType <= 3) {
          AutoCompare_setStep(auto_compare, modify_step_optType, modify_step_value);
          modify_step_optType = 0;
        }

        //event: 更改cooling_time
        if (1 <= modify_cooling_time_optType && modify_cooling_time_optType <=3) {
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
            send_error_bluetooth(0x00);
          }

          modify_cooling_time_optType = 0;
        }

        // 如果不处于暂停，则进行下一步
        if (PAUSE == 0) {
          StateMachine = TRIG_UP_AND_DOWN;
        }
        break;


      case TRIG_UP_AND_DOWN:
        // 正在倒计时
        if (OrdinaryTimer_is_Counting(main_cycle_timer)) break;
        
        // 刚进入此状态，上拉Trig+开始倒计时
        if (OrdinaryTimer_is_Reset(main_cycle_timer)) {
          CaptureTimer_mark_CaptureCplt(capture_timer, 0);
          CaptureTimer_TriggerUp(capture_timer);
          OrdinaryTimer_SetCycle_ms(main_cycle_timer, TRIG_DURATION_ms);
        }

        // 倒计时完毕，将Trig下拉，开始捕获
        if (OrdinaryTimer_is_CountCplt(main_cycle_timer)) {
          TimerEp_SetCounter((Timer *)capture_timer, 0); // 将TIM1清零，防止测量时自动重装载
          CaptureTimer_TriggerDown(capture_timer); // 下拉既代表开始捕获
          OrdinaryTimer_Reset(main_cycle_timer); // 重置计时器
          StateMachine = MEASURING;
        }
        break;


      case MEASURING:
        // 如果捕获完毕
        if (CaptureTimer_is_CaptureCplt(capture_timer)) {
          StateMachine = PROCESSING;
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
        StateMachine = ROTATE;
        break;


      case ROTATE:
        PwmTimer_SetCompare(
          servo_pwm_timer,
          AutoCompare_next(auto_compare)
        );
        OrdinaryTimer_Reset(main_cycle_timer);
        OLED_REFRESH_REQUEST = 1;
        osMessageQueuePut(sensor_dataHandle, &distance, 0, 0);
        osMessageQueuePut(sensor_dataHandle, &angle, 0, 0);
        StateMachine = COOLING;
        break;


      case COOLING:
        // 正在倒计时
        if (OrdinaryTimer_is_Counting(main_cycle_timer)) break;

        // 刚进入COOLING，开始倒计时
        if (OrdinaryTimer_is_Reset(main_cycle_timer)) {
          OrdinaryTimer_SetCycle_ms(main_cycle_timer, cooling_time_ms);
        }

        // 倒计时完毕
        if (OrdinaryTimer_is_CountCplt(main_cycle_timer)) {
          OrdinaryTimer_Reset(main_cycle_timer);
          StateMachine = PAUSE_AND_EVENT_HANDLE;
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
    if (KEY2_IS_PRESSED()) {
      HAL_Delay(10);
      if (KEY2_IS_PRESSED()) {
        LED_toggle(led_blue);
        ENABLE_OLED = 1 - ENABLE_OLED;

        if (ENABLE_OLED) OLED_REFRESH_REQUEST = 1;
        else OLED_CLEAR_REQUEST = 1;
            
        while (KEY2_IS_PRESSED());
      }
    }

    // event: 清空或显示OLED
    if (OLED_CLEAR_REQUEST) {
      OLED_CLEAR_REQUEST = 0;
      OLED_NewFrame();
      OLED_ShowFrame();
    }
    else if (OLED_REFRESH_REQUEST && ENABLE_OLED) {
      OLED_REFRESH_REQUEST = 0;
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
    if (Usart_is_RxCplt(bluetooth)) {
      Usart_mark_RxCplt(bluetooth, 0);
      bluetooth_cmd_handler(bluetooth);
      Usart_ReceiveToIdle_DMA_noHT(bluetooth);
    }

    if (Usart_is_TxCplt(bluetooth)
        && osMessageQueueGetCount(sensor_dataHandle) == 2
       )
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

