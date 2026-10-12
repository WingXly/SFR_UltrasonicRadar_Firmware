/**
 * @file led.c
 * @brief GPIO引脚上LED小灯的面向对象封装
 * @anchor WingXly
 * @version 1.0
 *
 * @attention
 * 本文件为LED小灯的面向对象封装
 *
 * @note
 * 使用流程:
 * 1. 在任意位置声明一个LED小灯：`LED * my_led = NULL;`
 * 2. 在main()或其他函数中实例化，例如：`my_led = LED_Instantiate(MY_LED_GPIO_PORT, MY_LED_PIN);`
 * 3. 在实例化之后使用函数控制小灯的开关，例如：`LED_on(my_led);`
 * 4. 当无需使用一个实例时，将它销毁：`LED_Free(my_led);`
 *
 * @note
 * 为保持封装，务必要在一个函数中实例化，而不是在将对象声明为全局变量时直接实例化。
 *
 */
#include "led.h"
#include <stdlib.h>

struct LED {
    GPIO_TypeDef *port;
    uint16_t pin;
};


LED * LED_Instantiate(GPIO_TypeDef * port, uint16_t pin) {
  LED *this = malloc(sizeof(LED));
  if (!this) return NULL;
  this->port = port;
  this->pin = pin;
  return this;
}
void LED_Free(LED * this) { free(this); }


void LED_on(LED * this) {
    HAL_GPIO_WritePin(this->port, this->pin, GPIO_PIN_SET);
}


void LED_off(LED * this) {
    HAL_GPIO_WritePin(this->port, this->pin, GPIO_PIN_RESET);
}

void LED_toggle(LED * this) {
    HAL_GPIO_TogglePin(this->port, this->pin);
}