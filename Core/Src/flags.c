#include "flags.h"
#include <stdint.h>




bool pause = false;
bool enable_oled = false;
bool oled_refresh_req = false;
bool oled_clear_req = false;
float cooling_time_ms = 30.0f;
uint8_t resample = 0;
uint8_t resample_remain = 0;


Usart * bluetooth = NULL;
LED * led_red   = NULL;
LED * led_green = NULL;
LED * led_blue  = NULL;
CaptureTimer  * capture_timer    = NULL;
OrdinaryTimer * main_cycle_timer = NULL;
PwmTimer      * servo_pwm_timer  = NULL;