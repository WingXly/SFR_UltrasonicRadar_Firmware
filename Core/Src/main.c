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
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_tim.h"
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

// 声速
#define SPEED_OF_SOUND_m_s 340.0f

// HC-SR04的触发用时
#define TRIG_DURATION_ms 0.01f

// PWM舵机最小占空比
#define PWMSERVO_MIN_COMPARE 50

// PWM舵机最大占空比
#define PWMSERVO_MAX_COMPARE 250

// PWM舵机的角度范围
#define PWMSERVO_ANGLE_RANGE 180.0f


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

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


/*PwmServo_Timer的compare控制器*/
 typedef struct {
  int compare;
  int multiplier;
  int step;
  int compare_min; int compare_max;
} AutoCompare_TypeDef;

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

  // TIM2的OLED展示
  if (htim->Instance == TimerEp_GetInstance((Timer*)OledCycle_Timer)) {
    if (!PAUSE) {
      OLED_REFRESH_REQUEST = 1;
    }
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

  float distance = 0;          // 距离（计算值）
  float angle = 0;             // 角度（计算值）
  AutoCompare_TypeDef ac_ = {
    .compare     = PWMSERVO_MIN_COMPARE,
    .multiplier  = 1,
    .step        = 1,
    .compare_min = PWMSERVO_MIN_COMPARE,
    .compare_max = PWMSERVO_MAX_COMPARE
  };
  AutoCompare_TypeDef * auto_compare = &ac_;

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
  /* ---------------------------------------- */

  HAL_Delay(20);
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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
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

        HAL_Delay(100);
        StateMachine = PAUSE_AND_EVENT_HANDLE;
        break;


      case PAUSE_AND_EVENT_HANDLE:
        // event: 按按钮（KEY1）暂停或解除暂停
        if (KEY1_IS_PRESSED()) {
          HAL_Delay(10);
          if (KEY1_IS_PRESSED()) {
            LED_toggle(LedGreen);
            LED_toggle(LedRed);
            PAUSE = 1 - PAUSE;
            
            while (KEY1_IS_PRESSED());
          }
        }

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
        else if (OLED_REFRESH_REQUEST == 1 && ENABLE_OLED == 1) {
          print_sensor_data_on_oled(distance, angle);
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
