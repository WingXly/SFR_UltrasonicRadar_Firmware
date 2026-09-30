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
#include "stm32f1xx_hal_uart.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "oled.h"
#include "font.h"
#include <stdio.h>
#include <sys/_intsup.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 声速
#define SPEED_OF_SOUND_m_s 340.0

// TIM3自动重装载间隔
#define TIM3_PERIOD_us 10

// 冷却时间
#define COOLING_TIME_us 30000

// 舵机的PWM步进时间
#define RADAR_PWM_STEP_ms 0.025f

// HC-SR04的触发用时
#define TRIG_DURATION 10

// 是否启用OLED输出
#define ENABLE_OLED_PRINT 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 状态机
volatile static StateMachine_sample StateMachine = IDLE; 
static int Pause_flag = -1;                  // 暂停标志位
static int Enable_oled_print_flag = -1;      // 启用OLED屏幕输出标志位
static int Oled_refresh_request_flag = 0;    // 请求OLED屏幕刷新标志位
volatile int Usart3_txCplx_flag = 0;         // 串口3发送完成标志位
volatile int Usart3_rxCplx_flag = 0;         // 串口3接收完成标志位

// 用于TIM3计时器的变量
volatile int tim3_counter = 0;               // 剩余时间（us）
volatile int tim3_counter_AC_flag = 0;       // 倒计时结束标志

// 用于TIM1捕获的变量
volatile int up_edge_tim1 = 0;               // 上升沿所对应的计数器值
volatile int down_edge_tim1 = 0;             // 下降沿所对应的计数器值

// 用于TIM4的PWM控制的变量
float valid_level_duration_tim4 = 0.5;       // PWM有效电平时间(ms)
int valid_level_duration_tim4_up_signal = 1; // PWM有效电平时间(ms)的上升标志位

// LED小灯。main函数里赋值
LED * LED_RED = NULL;
LED * LED_GREEN = NULL;
LED * LED_BLUE = NULL;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*设置HC-SR04触发器的output*/
void set_trig_pin(GPIO_PinState pin_state)
{
  HAL_GPIO_WritePin(HC_SR04_Trig_GPIO_Port, HC_SR04_Trig_Pin, pin_state);
}


/*设置TIM4的PWM占空比*/
void set_PWM_compare(int compare)
{
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, compare);
}


/*----------------------------------------------------------------------------------------------------*/


/*根据TIM1捕获计算距离*/
float calc_distance_mm(int up_edge_number, int down_edge_number)
{
  return (
    ( (float)down_edge_number - (float)up_edge_number ) * (float)SPEED_OF_SOUND_m_s / 2000.0f
  );
}


/*自动计算TIM4的下一个compare值*/
int calc_next_tim4_compare(void)
{
  if (valid_level_duration_tim4_up_signal == 1) {
    valid_level_duration_tim4 += RADAR_PWM_STEP_ms;
  } else {
    valid_level_duration_tim4 -= RADAR_PWM_STEP_ms;
  }

  if (valid_level_duration_tim4 >= 2.5f) {
    valid_level_duration_tim4 = 2.5f;
    valid_level_duration_tim4_up_signal = 0;
  }
  if (valid_level_duration_tim4 <= 0.5f) {
    valid_level_duration_tim4 = 0.5f;
    valid_level_duration_tim4_up_signal = 1;
  }

  return (int)(valid_level_duration_tim4 * 100.0f + 0.5f);
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

uint8_t sendbuffer[9]:
帧头         长度    角度     距离        校验      帧尾
0xAA 0x55    N      int16    uint16      XOR      0x0D
0    1       2      3   4    5    6      7        8

传入的角度单位：度
传入的距离单位：mm
*/
void send_auto_bluetooth(float distance_mm, float angle_deg)
{
  if (Usart3_txCplx_flag == 0) return;
  static uint8_t sendbuffer[9] = "";

  sendbuffer[0] = 0xAA;
  sendbuffer[1] = 0x55;
  sendbuffer[2] = 5;                                 // 长度
  // 以0.01度为单位发送角度数据
  sendbuffer[3] = (int16_t)(angle_deg * 100) >> 8;   // 角度高字节
  sendbuffer[4] = (int16_t)(angle_deg * 100) & 0xFF; // 角度低字节
  // 以1mm为单位发送距离数据
  sendbuffer[5] = (uint16_t)distance_mm >> 8;        // 距离高字节
  sendbuffer[6] = (uint16_t)distance_mm & 0xFF;      // 距离低字节
  sendbuffer[7] = sendbuffer[2] ^ sendbuffer[3] ^ sendbuffer[4] ^ sendbuffer[5] ^ sendbuffer[6]; // 校验
  sendbuffer[8] = 0x0D;                              // 帧尾

  Usart3_txCplx_flag = 0;
  HAL_UART_Transmit_DMA(&huart3, sendbuffer, 9);
  __HAL_DMA_DISABLE_IT(&hdma_usart3_tx, DMA_IT_HT); // 禁止半传输中断
  // $05: call
}


/*----------------------------------------------------------------------------------------------------*/

/*串口Tx回调函数*/
// $05: callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        Usart3_txCplx_flag = 1;
    }
}


/*计时器自动重装载回调函数。用于TIM3的us级计时和TIM2的OLED展示*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim3) {
    if (tim3_counter > 0) {
      tim3_counter--;
      if (tim3_counter == 0) tim3_counter_AC_flag = 1;
    }
  } 

  if (htim == &htim2) {
    if (StateMachine == IDLE || Pause_flag == 1) return;
    Oled_refresh_request_flag = 1;
  }
}


// $02: callback
/*计时器捕获处理函数。用于TIM1捕获上升和下降沿*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
    if (StateMachine != MEASURING) return;
    // 读取上升沿和下降沿的数值
    up_edge_tim1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
    down_edge_tim1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
    // 更新状态机
    if (StateMachine == MEASURING) {
      StateMachine = PROCESSING;
      // $03: call
    }
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

  // 初始化OLED
  HAL_Delay(20);
  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, "Start", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();

  // 初始化TIM1
  HAL_TIM_Base_Start(&htim1);
  HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_4);
  float distance = 0;          // 距离（计算值）

  // 初始化TIM2
  HAL_TIM_Base_Start_IT(&htim2);

  // 初始化TIM3
  HAL_TIM_Base_Start_IT(&htim3);

  // 初始化TIM4
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  int tim4_compare = 0;        // TIM4（PWM）的compare值
  float angle = 0;             // 角度（计算值）

  // 实例化LED小灯
  LED_RED = LED_Instantiate(LED_RED_GPIO_Port, LED_RED_Pin);
  LED_GREEN = LED_Instantiate(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  LED_BLUE = LED_Instantiate(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // event: 暂停
    if (KEY1_IS_PRESSED()) {
      HAL_Delay(10);
      if (KEY1_IS_PRESSED() && Pause_flag != -1) {
        LED_toggle(LED_GREEN);
        LED_toggle(LED_RED);
        Pause_flag = 1 - Pause_flag;
        
        while (KEY1_IS_PRESSED());
      }
    }


    // event: 启用或禁用OLED输出
    if (KEY2_IS_PRESSED()) {
      HAL_Delay(10);
      if (KEY2_IS_PRESSED() && Enable_oled_print_flag != -1) {
        LED_toggle(LED_BLUE);
        Enable_oled_print_flag = 1 - Enable_oled_print_flag;

        OLED_NewFrame();
        if (Enable_oled_print_flag) print_auto_OLED(distance, angle);
        OLED_ShowFrame();
        
        while (KEY2_IS_PRESSED());
      }
    }


    // 遍历状态机
    switch (StateMachine)
    {
      case IDLE:
        // 阻止HC-SR04在IDLE状态下工作
        set_trig_pin(GPIO_PIN_RESET);

        // 启动时暂停
        LED_off(LED_GREEN);
        LED_on(LED_RED);
        Pause_flag = 1;

        // 启动时不启用OLED输出
        LED_off(LED_BLUE);
        Oled_refresh_request_flag = 0;
        Enable_oled_print_flag = 0;

        // 允许状态机步进
        tim3_counter = 0;
        tim3_counter_AC_flag = 0;

        // 启动时将舵机置于0度
        set_PWM_compare(50);
        StateMachine = TRIG_UP_AND_DOWN;

        // 启用蓝牙发送
        Usart3_txCplx_flag = 1;

        HAL_Delay(500);
        break;


      case TRIG_UP_AND_DOWN:
        // 在暂停时,将状态机卡在TRIG_UP_AND_DOWN
        if (Pause_flag == 1) break;

        // 正在倒计时
        if (tim3_counter != 0) break;
        
        // 刚进入此状态
        if (tim3_counter_AC_flag == 0) {
          set_trig_pin(GPIO_PIN_SET);
          tim3_counter = TRIG_DURATION/TIM3_PERIOD_us;
          // $01 call
        }
        break;


      case MEASURING:
        break;


      case PROCESSING:
      // $03: callback
        distance = calc_distance_mm(up_edge_tim1, down_edge_tim1);
        angle = (valid_level_duration_tim4 - 0.5) * 90;
        if (ENABLE_OLED_PRINT && Oled_refresh_request_flag == 1 && Enable_oled_print_flag == 1) {
          print_auto_OLED(distance, angle);
        }
        StateMachine = ROTATE;
        break;


      case ROTATE:
        set_PWM_compare(tim4_compare);
        tim4_compare = calc_next_tim4_compare();
        tim3_counter = 0;
        tim3_counter_AC_flag = 0;
        StateMachine = COOLING;
        break;

        
      case COOLING:
        // 正在计数
        if (tim3_counter != 0) break;

        // 刚进入COOLING
        if (tim3_counter_AC_flag == 0) {
          send_auto_bluetooth(distance, angle);
          tim3_counter = COOLING_TIME_us / TIM3_PERIOD_us;
        }
        break;
        // $04 call
    }


    // $01 callback
    if (StateMachine == TRIG_UP_AND_DOWN && tim3_counter_AC_flag == 1) {
        set_trig_pin(GPIO_PIN_RESET);
        __HAL_TIM_SET_COUNTER(&htim1, 0); // 将TIM1清零，防止测量时自动重装载
        tim3_counter_AC_flag = 0;
        StateMachine = MEASURING;
        //call: 02
    }


    // $04 callback
    if (StateMachine == COOLING && tim3_counter_AC_flag == 1) {
      tim3_counter_AC_flag = 0;
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
