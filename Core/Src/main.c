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
#include "cmsis_os.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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

/* USER CODE BEGIN PV */

// 全局标志位
volatile StateMachine_TypeDef StateMachine = INIT; // 状态机

volatile int PAUSE = 0;                            // 是否暂停（IDLE默认暂停）
volatile int ENABLE_OLED = 0;                      // 是否启用OLED输出（IDLE默认不启用）

volatile int OLED_REFRESH_REQUEST = 0;             // 请求OLED屏幕刷新标志位
volatile int OLED_CLEAR_REQUEST = 0;               // 请求OLED屏幕清空标志位

volatile int MODIFY_STEP_OPT_TYPE = 0;             // 修改step值的操作类型（1:set 2:add 3:remove）
volatile int MODIFY_STEP_VALUE = 1;                // 修改step值的具体数字

// 冷却时间
volatile float cooling_time_ms = 30.0f;

// 蓝牙串口
Usart * BlueTooth = NULL;

// LED小灯
LED * LedRed   = NULL;
LED * LedGreen = NULL;
LED * LedBlue  = NULL;

// 定时器
CaptureTimer  * Capture_Timer   = NULL;
OrdinaryTimer * OledCycle_Timer = NULL;
OrdinaryTimer * MainCycle_Timer = NULL;
PwmTimer      * ServoPwm_Timer  = NULL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */


/*根据TIM1捕获计算距离*/
float calc_distance_mm(int up_edge_number, int down_edge_number)
{
  return (
    (float)(down_edge_number - up_edge_number) * (float)SPEED_OF_SOUND_m_s / 2000.0f
  );
}


/*将距离和角度输出在OLED上*/
void print_sensor_data_on_oled(float distance, float angle)
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


/*计算下一个compare值并返回计算结果*/
int AutoCompare_next(AutoCompare_TypeDef * this)
{
  if (this->compare >= this->compare_max) {
    this->compare = this->compare_max;
    this->multiplier = -1;
  }
  else if (this->compare <= this->compare_min) {
    this->compare = this->compare_min;
    this->multiplier = 1;
  }
  this->compare += this->multiplier * this->step;
  return this->compare;
}

/*设置compare步进值（operation_type: set, add, remove）（不允许step为负数）*/
void AutoCompare_setStep( AutoCompare_TypeDef * this,
                          int operation_type, int value )
{
  if (operation_type == 1) {
    this->step = value;
  } else if (operation_type == 2) {
    this->step += value;
  } else if (operation_type == 3) {
    this->step -= value;
  }
  
  if (this->step < 0) this->step = 0;
}


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/*计时器自动重装载回调函数*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // TIM3的us级计时
  if (htim->Instance == TimerEp_GetInstance((Timer*)MainCycle_Timer)) {
    OrdinaryTimer_CountDown(MainCycle_Timer);
  } 
}


/*计时器捕获处理函数*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  // TIM1捕获上升和下降沿
  if ( htim->Instance == TimerEp_GetInstance((Timer*)Capture_Timer)
       && htim->Channel == CaptureTimer_GetActiveChannel(Capture_Timer)
       && StateMachine == MEASURING )
  {
    CaptureTimer_SaveCounter_both(Capture_Timer);
    CaptureTimer_Mark_CaptureCplt(Capture_Timer, 1);
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

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
