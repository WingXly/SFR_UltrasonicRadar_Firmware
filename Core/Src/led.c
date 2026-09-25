#include "led.h"
#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include <stdlib.h>

struct LED {
    GPIO_TypeDef *port;
    uint16_t pin;
};


LED * LED_Instantiate(GPIO_TypeDef *port, uint16_t pin) {
  LED *this = malloc(sizeof(LED));
  if (!this) return NULL;
  this->port = port;
  this->pin = pin;
  return this;
}
void LED_Free(LED *this) { free(this); }


void LED_on(LED *this) {
    HAL_GPIO_WritePin(this->port, this->pin, GPIO_PIN_SET);
}


void LED_off(LED *this) {
    HAL_GPIO_WritePin(this->port, this->pin, GPIO_PIN_RESET);
}