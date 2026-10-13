/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/*from main.c*/

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

float calc_distance_mm(int up_edge_number, int down_edge_number);
void print_sensor_data_on_oled(float distance, float angle);

// class AutoCompare_TypeDef {
      typedef struct AutoCompare_TypeDef AutoCompare_TypeDef;

      int AutoCompare_next(AutoCompare_TypeDef * this);
      void AutoCompare_setStep( AutoCompare_TypeDef * this,
                                int operation_type, int value );
// }
extern AutoCompare_TypeDef * auto_compare;

/*from freertos.c*/

void awake_bt_send_data(float data1, float data2);
void awake_bt_send_error(uint16_t error_code);
void awake_oled_update(bool oled_is_enable);
void awake_main_task();

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_BLUE_Pin GPIO_PIN_6
#define LED_BLUE_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_7
#define LED_GREEN_GPIO_Port GPIOA
#define LED_RED_Pin GPIO_PIN_0
#define LED_RED_GPIO_Port GPIOB
#define KEY1_Pin GPIO_PIN_12
#define KEY1_GPIO_Port GPIOB
#define KEY1_EXTI_IRQn EXTI15_10_IRQn
#define KEY2_Pin GPIO_PIN_13
#define KEY2_GPIO_Port GPIOB
#define KEY2_EXTI_IRQn EXTI15_10_IRQn
#define HC_SR04_Trig_Pin GPIO_PIN_11
#define HC_SR04_Trig_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

#define KEY1_IS_PRESSED() (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
#define KEY2_IS_PRESSED() (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
