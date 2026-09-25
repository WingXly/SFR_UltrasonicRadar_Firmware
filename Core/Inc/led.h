#ifndef __LED_H
#define __LED_H
#include "main.h"
#include "stm32f1xx_hal.h"

typedef struct LED LED;

LED * LED_obj_init(GPIO_TypeDef *port, uint16_t pin);
void LED_obj_deinit(LED *this);
void LED_on(LED *this);
void LED_off(LED *this);

#endif // __LED_H