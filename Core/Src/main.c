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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SPEED_OF_SOUND_m_s 340.0
#define TIM3_PERIOD_us 10
#define COOLING_TIME_us 10000
#define RADAR_PWM_STEP_ms 0.05f

#define KEY1_IS_PRESSED() (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 状态机
volatile StateMachine_sample StateMachine = IDEL; 
int PauseFlag = 1;                           // 暂停标志位

// 用于TIM3计时器的变量
volatile int counter_tim3 = 0;               // 剩余时间（us）
volatile int counter_tim3_AC_signal = 0;     // 归零标志

// 用于TIM1捕获的变量
volatile int up_edge_tim1 = 0;               // 上升沿所对应的计数器值
volatile int down_edge_tim1 = 0;             // 下降沿所对应的计数器值

// 用于TIM4的PWM控制的变量
float valid_level_duration_tim4 = 0.5;       // PWM有效电平时间(ms)
int valid_level_duration_tim4_up_signal = 1; // PWM有效电平时间(ms)的上升标志位
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*设置HC-SR04触发器的output*/
void set_trig_pin(GPIO_PinState pin_state) {
  HAL_GPIO_WritePin(HC_SR04_Trig_GPIO_Port, HC_SR04_Trig_Pin, pin_state);
}


/*设置TIM4的PWM占空比*/
void set_PWM_compare(int compare) {
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, compare);
}


/*计时器自动重装载回调函数。用于TIM3的us级计时*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim == &htim3) {
    if (counter_tim3 > 0) {
      counter_tim3--;
      if (counter_tim3 == 0) counter_tim3_AC_signal = 1;
    }
  }
}


// callback: 02
/*计时器捕获处理函数。用于TIM1捕获上升和下降沿*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
    if (StateMachine != MEASURING) return;
    // 读取上升沿和下降沿的数值
    up_edge_tim1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
    down_edge_tim1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
    // 更新状态机
    if (StateMachine == MEASURING) {
      StateMachine = PROCESSING;
      //call: 03
    }
  }
}


/*----------------------------------------------------------------------------------------------------*/


/*根据TIM1捕获计算距离*/
float calc_distance_cm(int up_edge_number, int down_edge_number) {
  return (((float)down_edge_number - (float)up_edge_number) * (float)SPEED_OF_SOUND_m_s / 10000 / 2);
}

/*自动计算TIM4的下一个compare值*/
int change_compare_tim4(void) {
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
void print_auto_OLED(float distance, float angle) {
  OLED_NewFrame();
  char print_msg_distance[20] = "";
  sprintf(print_msg_distance, "d = %.2f", distance);
  OLED_PrintString(0, 0, print_msg_distance, &font16x16, OLED_COLOR_NORMAL);
  char print_msg_angle[20] = "";
  sprintf(print_msg_angle, "a = %.2f", angle);
  OLED_PrintString(0, 16, print_msg_angle, &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
}


/*将距离和角度通过蓝牙发送至电脑*/
void send_auto_bluetooth(float distance, float angle) {
  static char sendbuffer[50] = "";
  uint16_t len = sprintf(sendbuffer, "d=%.2f,a=%.2f\r\n", distance, angle);
  if (huart2.gState != HAL_UART_STATE_READY) return;
  HAL_UART_Transmit_DMA(&huart2, (uint8_t*)sendbuffer, len);
}


/*控制小灯亮灭*/
void green_LED(GPIO_PinState state) {
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, state);
}
void red_LED(GPIO_PinState state) {
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, state);
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
  MX_USART2_UART_Init();
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
  float distance;

  // 初始化TIM3
  HAL_TIM_Base_Start_IT(&htim3);

  // 初始化TIM4
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  int compare_tim4 = 0;
  float angle = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // event: 暂停确认
    if (KEY1_IS_PRESSED() && PauseFlag == 0) {
      HAL_Delay(10);
      if (KEY1_IS_PRESSED()) {
        red_LED(GPIO_PIN_SET);
        green_LED(GPIO_PIN_RESET);
        PauseFlag = 1;
      }
      while (KEY1_IS_PRESSED());
    }

    // 遍历状态机
    switch (StateMachine) {
      case IDEL:
        set_trig_pin(GPIO_PIN_RESET);
        green_LED(GPIO_PIN_RESET);
        red_LED(GPIO_PIN_SET);
        PauseFlag = 1;

        counter_tim3 = 0;
        counter_tim3_AC_signal = 0;

        set_PWM_compare(50);
        StateMachine = TRIG_UP_AND_DOWN;
        HAL_Delay(500);
        break;

      case TRIG_UP_AND_DOWN:
        //event: 暂停取消
        while (PauseFlag == 1) {
          if (KEY1_IS_PRESSED()) {
              HAL_Delay(10);
              if (KEY1_IS_PRESSED()) {
                red_LED(GPIO_PIN_RESET);
                green_LED(GPIO_PIN_SET);
                PauseFlag = 0;
              }
              while (KEY1_IS_PRESSED());
          }
        }

        // 正在计数
        if (counter_tim3 != 0) break;

        // 刚进入此状态
        if (counter_tim3_AC_signal == 0) {
          set_trig_pin(GPIO_PIN_SET);
          // 让TIM3计时50us
          counter_tim3 = 50/TIM3_PERIOD_us;
          // call: 01
        }
        break;

      case MEASURING:
        break;

      case PROCESSING: //callback: 03
        distance = calc_distance_cm(up_edge_tim1, down_edge_tim1);
        angle = (valid_level_duration_tim4 - 0.5) * 90;
        print_auto_OLED(distance, angle);
        StateMachine = ROTATE;
        break;

      case ROTATE:
        set_PWM_compare(compare_tim4);
        compare_tim4 = change_compare_tim4();
        counter_tim3 = 0;
        counter_tim3_AC_signal = 0;
        StateMachine = COOLING;
        break;

      case COOLING:
        // 正在计数
        if (counter_tim3 != 0) break;

        // 刚进入COOLING
        if (counter_tim3_AC_signal == 0) {
          send_auto_bluetooth(distance, angle);
          counter_tim3 = COOLING_TIME_us / TIM3_PERIOD_us;
        }
        break;
        // call: 04
    }

    // callback: 01
    if (StateMachine == TRIG_UP_AND_DOWN && counter_tim3_AC_signal == 1) {
        set_trig_pin(GPIO_PIN_RESET);
        __HAL_TIM_SET_COUNTER(&htim1, 0); // 将TIM1清零，防止测量时自动重装载
        counter_tim3_AC_signal = 0;
        StateMachine = MEASURING;
        //call: 02
    }

    // callback: 04
    if (StateMachine == COOLING && counter_tim3_AC_signal == 1) {
      counter_tim3_AC_signal = 0;
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
