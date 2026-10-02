/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_uart.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "oled.h"
#include "font.h"
#include "timer_ep.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// 声速
#define SPEED_OF_SOUND_m_s 340.0f

// HC-SR04的触发用时
#define TRIG_DURATION_ms 0.01f

// 冷却时间
float COOLING_TIME_ms = 30.0f;

// PWM的compare步进
int PWM_COMPARE_STEP = 1;

// 通讯协议
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0x55
#define FRAME_END 0xD0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// 状态与标志
volatile static StateMachine_sample StateMachine = IDLE; // 状态机
volatile static int Pause_flag = -1;                     // 暂停标志位
volatile static int Enable_oled_flag = 1;                         // 是否启用OLED输出
volatile static int Oled_refresh_request_flag = 0;       // 请求OLED屏幕刷新标志位
volatile int UsartBluetooth_txCplx_flag = 0;             // 蓝牙串口发送完成标志位
volatile int UsartBluetooth_rxCplx_flag = 0;             // 蓝牙串口接收完成标志位

// 用于捕获定时器的变量
volatile int upEdge_counter = 0;                         // 上升沿所对应的计数器值
volatile int downEdge_counter = 0;                       // 下降沿所对应的计数器值

// 用于PWM控制的变量
int autoCompare = 0;                                     // PWM计时器的compare值
int autoCompare_multiplier = 1;                          // autoCompare的乘数 (1 或 -1)

// LED小灯
LED * LedRed   = NULL;
LED * LedGreen = NULL;
LED * LedBlue  = NULL;

// 蓝牙串口
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;
Usart Usart_bluetooth = {
  .huart = &huart3,
  .DMA_handle_rx = &hdma_usart3_rx,
  .DMA_handle_tx = &hdma_usart3_tx,
};

// 定时器
CaptureTimer  * Capture_Timer   = NULL;
OrdinaryTimer * OledCycle_Timer = NULL;
OrdinaryTimer * MainCycle_Timer = NULL;
PwmTimer      * ServoPwm_Timer  = NULL;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*根据TIM1捕获计算距离*/
float calc_distance_mm(int up_edge_number, int down_edge_number)
{
  return (
    ( (float)down_edge_number - (float)up_edge_number ) * (float)SPEED_OF_SOUND_m_s / 2000.0f
  );
}


/*自动计算TIM4的下一个compare值并返回它*/
int autoCompare_next(void)
{
  autoCompare += (autoCompare_multiplier * PWM_COMPARE_STEP);

  int ARR = TimerEp_GetArr((Timer*)ServoPwm_Timer);
  int autoCompare_max = (int)(ARR * 0.125f + 0.5f); // 向上取整+0.5
  int autoCompare_min = (int)(ARR * 0.025f + 0.5f);

  if (autoCompare >= autoCompare_max) {
    autoCompare = autoCompare_max;
    autoCompare_multiplier = -1;
  }
  else if (autoCompare <= autoCompare_min) {
    autoCompare = autoCompare_min;
    autoCompare_multiplier = 1;
  }

  return autoCompare;
}


/*将距离和角度输出在OLED上*/
void print_auto_OLED(float distance, float angle)
{
  OLED_NewFrame();
  char printMsg_distance[20] = "";
  sprintf(printMsg_distance, "d = %.2f", distance);
  OLED_PrintString(0, 0, printMsg_distance, &font16x16, OLED_COLOR_NORMAL);
  char printMsg_angle[20] = "";
  sprintf(printMsg_angle, "a = %.2f", angle);
  OLED_PrintString(0, 16, printMsg_angle, &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
}


/*
将距离和角度通过串口发送至电脑。采用二进制帧结构发布通知：

uint8_t sendMsg[9]:
帧头         长度    角度     距离        校验      帧尾
0xAA 0x55    N      int16    uint16      XOR      0x0D
0    1       2      3   4    5    6      7        8

角度单位：度 -> 0.01度
距离单位：mm -> mm
*/
void send_auto_bluetooth(float distance_mm, float angle_deg)
{
  if (UsartBluetooth_txCplx_flag == 0) return;
  static uint8_t sendMsg[9] = "";

  sendMsg[0] = FRAME_HEADER_1;
  sendMsg[1] = FRAME_HEADER_2;
  sendMsg[2] = 5;                                 // 长度
  // 以0.01度为单位发送角度数据
  sendMsg[3] = (int16_t)(angle_deg * 100) >> 8;   // 角度高字节
  sendMsg[4] = (int16_t)(angle_deg * 100) & 0xFF; // 角度低字节
  // 以1mm为单位发送距离数据
  sendMsg[5] = (uint16_t)distance_mm >> 8;        // 距离高字节
  sendMsg[6] = (uint16_t)distance_mm & 0xFF;      // 距离低字节
  sendMsg[7] = sendMsg[2] ^ sendMsg[3] ^ sendMsg[4] ^ sendMsg[5] ^ sendMsg[6]; // 校验
  sendMsg[8] = FRAME_END;                              // 帧尾

  UsartBluetooth_txCplx_flag = 0;
  HAL_UART_Transmit_DMA(Usart_bluetooth.huart, sendMsg, 9);
  __HAL_DMA_DISABLE_IT(Usart_bluetooth.DMA_handle_tx, DMA_IT_HT); // 禁止半传输中断
  // $05: call
}


/*----------------------------------------------------------------------------------------------------*/


// $05: callback
/*串口Tx回调函数*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Usart_bluetooth.huart) {
        UsartBluetooth_txCplx_flag = 1;
    }
}


/*计时器自动重装载回调函数*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // TIM3的us级计时
  if (htim->Instance == TimerEp_GetInstance((Timer*)MainCycle_Timer)) {
    TimerEp_OrdinaryTimer_CountDown(MainCycle_Timer);
  } 

  // TIM2的OLED展示
  if (htim->Instance == TimerEp_GetInstance((Timer*)OledCycle_Timer)) {
    if (StateMachine == IDLE || Pause_flag == 1) return;
    Oled_refresh_request_flag = 1;
  }
}


// $02: callback
/*计时器捕获处理函数*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  // TIM1捕获上升和下降沿
  if ( htim->Instance == TimerEp_GetInstance((Timer*)Capture_Timer)
       && htim->Channel == TimerEp_CaptureTimer_GetActiveChannel(Capture_Timer)
       && StateMachine == MEASURING )
  {
    upEdge_counter = TimerEp_CaptureTimer_ReadUpEdge(Capture_Timer);
    downEdge_counter = TimerEp_CaptureTimer_ReadDownEdge(Capture_Timer);
    StateMachine = PROCESSING;
    // $03: call
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  // 实例化LED小灯
  LedRed = LED_Instantiate(LED_RED_GPIO_Port, LED_RED_Pin);
  LedGreen = LED_Instantiate(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  LedBlue = LED_Instantiate(LED_BLUE_GPIO_Port, LED_BLUE_Pin);

  // 实例化定时器
  Capture_Timer = TimerEp_CaptureTimer_Instantiate(
    &htim1,
    72,
    HC_SR04_Trig_GPIO_Port,
    HC_SR04_Trig_Pin,
    TIM_CHANNEL_3,
    TIM_CHANNEL_4,
    HAL_TIM_ACTIVE_CHANNEL_4
  );

  OledCycle_Timer = TimerEp_OrdinaryTimer_Instantiate(
    &htim2,
    72
  );

  MainCycle_Timer = TimerEp_OrdinaryTimer_Instantiate(
    &htim3,
    72
  );

  ServoPwm_Timer = TimerEp_PwmTimer_Instantiate(
    &htim4,
    72,
    TIM_CHANNEL_3
  );

  // 确认所有对象创建成功
  if ( LedRed == NULL || LedGreen == NULL || LedBlue == NULL ||
       Capture_Timer == NULL || OledCycle_Timer == NULL || MainCycle_Timer == NULL || ServoPwm_Timer == NULL )
  {
    Error_Handler();
  }

  // 所有对象就绪之后，允许中断访问它们
  if ( HAL_TIM_Base_Start(&htim1) != HAL_OK 
       || HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_3) != HAL_OK
       || HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_4) != HAL_OK
       || HAL_TIM_Base_Start_IT(&htim2) != HAL_OK
       || HAL_TIM_Base_Start_IT(&htim3) != HAL_OK
       || HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3) != HAL_OK )
  {
    Error_Handler();
  }

  // 初始化OLED
  HAL_Delay(20);
  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, "Start", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();

  float distance = 0;          // 距离（计算值）
  int autoCompare = 0;         // TIM4（PWM）的compare值
  float angle = 0;             // 角度（计算值）

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // event: 暂停
    if (KEY1_IS_PRESSED()) {
      HAL_Delay(10);
      if (KEY1_IS_PRESSED() && Pause_flag != -1) {
        LED_toggle(LedGreen);
        LED_toggle(LedRed);
        Pause_flag = 1 - Pause_flag;
        
        while (KEY1_IS_PRESSED());
      }
    }


    // event: 启用或禁用OLED输出
    if (KEY2_IS_PRESSED()) {
      HAL_Delay(10);
      if (KEY2_IS_PRESSED()) {
        LED_toggle(LedBlue);
        Enable_oled_flag = 1 - Enable_oled_flag;

        OLED_NewFrame();
        if (Enable_oled_flag) print_auto_OLED(distance, angle);
        OLED_ShowFrame();
        
        while (KEY2_IS_PRESSED());
      }
    }


    // 遍历状态机
    switch (StateMachine)
    {
      case IDLE:
        // 阻止HC-SR04在IDLE状态下工作
        TimerEp_CaptureTimer_TriggerDown(Capture_Timer);

        // 启动时暂停
        LED_off(LedGreen);
        LED_on(LedRed);
        Pause_flag = 1;

        // 启动时不启用OLED输出
        LED_off(LedBlue);
        Oled_refresh_request_flag = 0;
        Enable_oled_flag = 0;

        // 允许状态机步进
        TimerEp_OrdinaryTimer_Reset(MainCycle_Timer);

        // 启动时将舵机置于0度
        TimerEp_PwmTimer_SetCompareRate(ServoPwm_Timer, 0.025);
        StateMachine = TRIG_UP_AND_DOWN;

        // 启用蓝牙发送
        UsartBluetooth_txCplx_flag = 1;

        HAL_Delay(500);
        break;


      case TRIG_UP_AND_DOWN:
        // 在暂停时,将状态机卡在TRIG_UP_AND_DOWN
        if (Pause_flag == 1) break;

        // 正在倒计时
        if (TimerEp_OrdinaryTimer_If_Counting(MainCycle_Timer)) break;
        
        // 刚进入此状态
        if (TimerEp_OrdinaryTimer_If_Reset(MainCycle_Timer)) {
          TimerEp_CaptureTimer_TriggerUp(Capture_Timer);
          TimerEp_OrdinaryTimer_SetCycle_ms(MainCycle_Timer, TRIG_DURATION_ms);
          // $01 call
        }
        break;


      case MEASURING:
        break;


      case PROCESSING:
      // $03: callback
        distance = calc_distance_mm(upEdge_counter, downEdge_counter);
        angle = (TimerEp_PwmTimer_GetCompareRate(ServoPwm_Timer) - 0.025) * 10 * 180;
        if (Oled_refresh_request_flag == 1 && Enable_oled_flag == 1 ) print_auto_OLED(distance, angle);
        StateMachine = ROTATE;
        break;


      case ROTATE:
        autoCompare = autoCompare_next();
        TimerEp_PwmTimer_SetCompare(ServoPwm_Timer, autoCompare);
        TimerEp_OrdinaryTimer_Reset(MainCycle_Timer);
        StateMachine = COOLING;
        break;

        
      case COOLING:
        // 正在计数
        if ( TimerEp_OrdinaryTimer_If_Counting(MainCycle_Timer) ) break;

        // 刚进入COOLING
        if ( TimerEp_OrdinaryTimer_If_Reset(MainCycle_Timer) ) {
          send_auto_bluetooth(distance, angle);
          TimerEp_OrdinaryTimer_SetCycle_ms(MainCycle_Timer, COOLING_TIME_ms);
        }
        break;
        // $04 call
    }


    // $01 callback
    if ( StateMachine == TRIG_UP_AND_DOWN && TimerEp_OrdinaryTimer_If_AC(MainCycle_Timer) ) {
        TimerEp_CaptureTimer_TriggerDown(Capture_Timer);
        TimerEp_SetCounter((Timer *)Capture_Timer, 0); // 将TIM1清零，防止测量时自动重装载
        TimerEp_OrdinaryTimer_Reset(MainCycle_Timer);
        StateMachine = MEASURING;
        // $02: call
    }


    // $04 callback
    if ( StateMachine == COOLING && TimerEp_OrdinaryTimer_If_AC(MainCycle_Timer) ) {
      TimerEp_OrdinaryTimer_Reset(MainCycle_Timer);
      StateMachine = TRIG_UP_AND_DOWN;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
