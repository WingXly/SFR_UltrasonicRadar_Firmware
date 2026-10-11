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
extern volatile StateMachine_TypeDef StateMachine; // 状态机

extern volatile int PAUSE;                         // 是否暂停（IDLE默认暂停）
extern volatile int ENABLE_OLED;                   // 是否启用OLED输出（IDLE默认不启用）

extern volatile int OLED_REFRESH_REQUEST;          // 请求OLED屏幕刷新标志位
extern volatile int OLED_CLEAR_REQUEST;            // 请求OLED屏幕清空标志位

extern volatile int MODIFY_STEP_OPT_TYPE;          // 修改step值的操作类型（1:set 2:add 3:remove）
extern volatile int MODIFY_STEP_VALUE;             // 修改step值的具体数字

extern volatile float cooling_time_ms;             // 冷却时间

// 蓝牙串口
extern Usart * BlueTooth;

// LED小灯
extern LED * LedRed;
extern LED * LedGreen;
extern LED * LedBlue;

// 定时器
extern CaptureTimer  * Capture_Timer;
extern OrdinaryTimer * OledCycle_Timer;
extern OrdinaryTimer * MainCycle_Timer;
extern PwmTimer      * ServoPwm_Timer;

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
/* Definitions for EventTask */
osThreadId_t EventTaskHandle;
const osThreadAttr_t EventTask_attributes = {
  .name = "EventTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal2,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void *argument);
void StartOledTask(void *argument);
void StartBluetoothTask(void *argument);
void StartEventTask(void *argument);

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

  /* creation of EventTask */
  EventTaskHandle = osThreadNew(StartEventTask, NULL, &EventTask_attributes);

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

  /* ---------------------------------------- */
  // 实例化LED小灯
  LedRed = LED_Instantiate(LED_RED_GPIO_Port, LED_RED_Pin);
  LedGreen = LED_Instantiate(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  LedBlue = LED_Instantiate(LED_BLUE_GPIO_Port, LED_BLUE_Pin);


  // 实例化定时器
  Capture_Timer = CaptureTimer_Instantiate(
    &htim1,
    72,
    HC_SR04_Trig_GPIO_Port,
    HC_SR04_Trig_Pin,
    TIM_CHANNEL_3,
    TIM_CHANNEL_4,
    HAL_TIM_ACTIVE_CHANNEL_4
  );

  OledCycle_Timer = OrdinaryTimer_Instantiate(
    &htim2,
    72
  );

  MainCycle_Timer = OrdinaryTimer_Instantiate(
    &htim3,
    72
  );

  ServoPwm_Timer = PwmTimer_Instantiate(
    &htim4,
    72,
    TIM_CHANNEL_3,
    1
  );

  // 实例化蓝牙串口
  extern DMA_HandleTypeDef hdma_usart3_rx;
  extern DMA_HandleTypeDef hdma_usart3_tx;
  BlueTooth = Usart_Instantiate(
    &huart3,
    &hdma_usart3_rx,
    &hdma_usart3_tx
  );

  // 启动外设
  osDelay(20);
  HAL_TIM_Base_Start(&htim1);
  HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_4);
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  OLED_Init();

  OLED_NewFrame();
  OLED_PrintString(0, 0, "Start", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
  /* ---------------------------------------- */

    AutoCompare_TypeDef ac_ = {
      .compare     = PWMSERVO_MIN_COMPARE,
      .multiplier  = 1,
      .step        = 1,
      .compare_min = PWMSERVO_MIN_COMPARE,
      .compare_max = PWMSERVO_MAX_COMPARE
    };
    AutoCompare_TypeDef * auto_compare = &ac_;

  /* Infinite loop */

  for(;;)
  {
    // 遍历状态机
    switch (StateMachine)
    {
      case INIT:
        // 阻止HC-SR04在INIT状态下工作
        CaptureTimer_TriggerDown(Capture_Timer);

        // 启动时暂停
        LED_off(LedGreen);
        LED_on(LedRed);
        PAUSE = 1;

        // 启动时不启用OLED输出
        LED_off(LedBlue);

        // 允许状态机步进
        OrdinaryTimer_Reset(MainCycle_Timer);

        // 启动时将舵机置于0度
        PwmTimer_SetDuty(ServoPwm_Timer, 0.025);

        // 启用蓝牙发送和接收
        Usart_Mark_RxCplt(BlueTooth, 1);
        Usart_Mark_TxCplt(BlueTooth, 1);

        // 等待命令
        Usart_ReceiveToIdle_DMA_noHT(BlueTooth);

        osDelay(100);
        StateMachine = PAUSE_AND_EVENT_HANDLE;
        break;


      case PAUSE_AND_EVENT_HANDLE:
        // event: 按按钮（KEY1）暂停或解除暂停
        if (KEY1_IS_PRESSED()) {
          osDelay(10);
          if (KEY1_IS_PRESSED()) {
            LED_toggle(LedGreen);
            LED_toggle(LedRed);
            PAUSE = 1 - PAUSE;
            
            while (KEY1_IS_PRESSED());
          }
        }

        // event: 更改compare step
         if (1 <= MODIFY_STEP_OPT_TYPE && MODIFY_STEP_OPT_TYPE <= 3) {
          AutoCompare_setStep(auto_compare, MODIFY_STEP_OPT_TYPE, MODIFY_STEP_VALUE);
          MODIFY_STEP_OPT_TYPE = 0;
         }

        // 如果不处于暂停，则进行下一步
        if (PAUSE == 0) {
          StateMachine = TRIG_UP_AND_DOWN;
        }
        break;


      case TRIG_UP_AND_DOWN:
        // 正在倒计时
        if (OrdinaryTimer_If_Counting(MainCycle_Timer)) break;
        
        // 刚进入此状态，上拉Trig+开始倒计时
        if (OrdinaryTimer_If_Reset(MainCycle_Timer)) {
          CaptureTimer_Mark_CaptureCplt(Capture_Timer, 0);
          CaptureTimer_TriggerUp(Capture_Timer);
          OrdinaryTimer_SetCycle_ms(MainCycle_Timer, TRIG_DURATION_ms);
        }

        // 倒计时完毕，将Trig下拉，开始捕获
        if (OrdinaryTimer_If_CountCplt(MainCycle_Timer)) {
          TimerEp_SetCounter((Timer *)Capture_Timer, 0); // 将TIM1清零，防止测量时自动重装载
          CaptureTimer_TriggerDown(Capture_Timer); // 下拉既代表开始捕获
          OrdinaryTimer_Reset(MainCycle_Timer); // 重置计时器
          StateMachine = MEASURING;
        }
        break;


      case MEASURING:
        // 如果捕获完毕
        if (CaptureTimer_If_CaptureCplt(Capture_Timer)) {
          StateMachine = PROCESSING;
        }
        break;


      case PROCESSING:
        distance = calc_distance_mm(
          CaptureTimer_LoadCounter_1(Capture_Timer),
          CaptureTimer_LoadCounter_2(Capture_Timer)
        );
        angle = PwmTimer_GetCompare_AsServoAngle(
          ServoPwm_Timer,
          PWMSERVO_MIN_COMPARE,
          PWMSERVO_MAX_COMPARE,
          PWMSERVO_ANGLE_RANGE
        );
        StateMachine = ROTATE;
        break;


      case ROTATE:
        PwmTimer_SetCompare(
          ServoPwm_Timer,
          AutoCompare_next(auto_compare)
        );
        OrdinaryTimer_Reset(MainCycle_Timer);
        OLED_REFRESH_REQUEST = 1;
        StateMachine = COOLING;
        break;

        
      case COOLING:
        // 正在倒计时
        if (OrdinaryTimer_If_Counting(MainCycle_Timer)) break;

        // 刚进入COOLING，开始倒计时
        if (OrdinaryTimer_If_Reset(MainCycle_Timer)) {
          send_auto_bluetooth(distance, angle);
          OrdinaryTimer_SetCycle_ms(MainCycle_Timer, cooling_time_ms);
        }

        // 倒计时完毕
        if (OrdinaryTimer_If_CountCplt(MainCycle_Timer)) {
          OrdinaryTimer_Reset(MainCycle_Timer);
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
        LED_toggle(LedBlue);
        ENABLE_OLED = 1 - ENABLE_OLED;

        if (ENABLE_OLED) {
          OLED_REFRESH_REQUEST = 1;
        } else {
          OLED_CLEAR_REQUEST = 1;
        }
            
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
    osDelay(1);
  }
  /* USER CODE END StartBluetoothTask */
}

/* USER CODE BEGIN Header_StartEventTask */
/**
* @brief Function implementing the EventTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartEventTask */
void StartEventTask(void *argument)
{
  /* USER CODE BEGIN StartEventTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartEventTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

