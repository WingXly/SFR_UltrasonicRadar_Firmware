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
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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

/* USER CODE BEGIN PV */

// 全局标志位

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


// class AutoCompare_TypeDef {}
struct AutoCompare_TypeDef{
        int compare;
        int multiplier;
        int step;
        int compare_min;
        int compare_max;
};

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
bool AutoCompare_setStep( AutoCompare_TypeDef * this,
                          int operation_type, int value )
{
  int step_old = this->step;

  if (operation_type == 1) {
    this->step = value;
  } else if (operation_type == 2) {
    this->step += value;
  } else if (operation_type == 3) {
    this->step -= value;
  }
  
  if (this->step < 0) {
    this->step = step_old;
    put_queue_btSendError_awake(0x00);
    return false;
  } 
  return true;
}

// 实例化AutoCompare
AutoCompare_TypeDef ac_ = {
  .compare     = PWMSERVO_MIN_COMPARE,
  .multiplier  = 1,
  .step        = 1,
  .compare_min = PWMSERVO_MIN_COMPARE,
  .compare_max = PWMSERVO_MAX_COMPARE
};
AutoCompare_TypeDef * auto_compare = &ac_;


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/*GPIO中断回调函数*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // 启用或禁用oled
  if (GPIO_Pin == KEY2_Pin) {
    LED_toggle(led_blue);
    enable_oled = !enable_oled;
  awake_oled_update(enable_oled);
  }

  // 启动或暂停
  else if (GPIO_Pin == KEY1_Pin) {
    LED_toggle(led_green);
    LED_toggle(led_red);
    pause = !pause;
  }
}


/*计时器自动重装载回调函数*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // TIM3的us级计时，为缓解中断压力，此处直接采用句柄而不走面向对象
  if (htim == &htim3) {
    // 倒计时
    OrdinaryTimer_CountDown(main_cycle_timer);

    // 倒计时结束则唤醒Task_Main
    if (OrdinaryTimer_is_CountCplt(main_cycle_timer)) {
      OrdinaryTimer_Reset(main_cycle_timer);
      main_cycle_done_awake();
    }
  } 
}


/*计时器捕获处理函数*/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  // TIM1捕获上升和下降沿
  if ( htim->Instance == TimerEp_GetInstance((Timer*)capture_timer)
       && htim->Channel == CaptureTimer_GetActiveChannel(capture_timer) )
  {
    // 记录捕获信息
    CaptureTimer_SaveCounter_both(capture_timer);
    
    // 唤醒Task_Main
    capture_done_awake();
  }
}


/*串口Tx回调函数*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  // 只对传输完成进行标注，不唤醒蓝牙任务
  if (huart == Usart_GetpHuart(bluetooth)) {
      Usart_mark_TxCplt(bluetooth, 1);
  }
}


/*串口Rx回调函数*/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart == Usart_GetpHuart(bluetooth)) {
    Usart_mark_RxCplt(bluetooth, 1);
    Usart_mark_RxSize(bluetooth, Size);
    awake_bt_task();
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
  /* USER CODE BEGIN 2 */


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


  // 启动外设
  HAL_Delay(20);

  CaptureTimer_Init(capture_timer);
  OrdinaryTimer_Init_IT(main_cycle_timer);
  PwmTimer_Init(servo_pwm_timer);

  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, "Start", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();


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
