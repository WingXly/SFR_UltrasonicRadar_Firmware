#include "flags.h"

StateMachine_TypeDef state_machine = INIT;


bool pause = false;
bool enable_oled = false;
bool oled_refresh_req = false;
bool oled_clear_req = false;
float cooling_time_ms = 30.0f;


int modify_step_optType = 0;
int modify_step_value = 0;
int modify_cooling_time_optType = 0;
int modify_cooling_time_value = 0;


Usart * bluetooth = NULL;
LED * led_red   = NULL;
LED * led_green = NULL;
LED * led_blue  = NULL;
CaptureTimer  * capture_timer    = NULL;
OrdinaryTimer * main_cycle_timer = NULL;
PwmTimer      * servo_pwm_timer  = NULL;