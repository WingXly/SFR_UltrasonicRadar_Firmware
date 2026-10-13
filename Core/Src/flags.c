#include "h"
#include "main.h"
#include <stdbool.h>


Flags_TypeDef flags = {
    .state_machine = INIT,
    .pause = false,
    .enable_oled = false,
    .oled_refresh_req = false,
    .oled_clear_req = false,
    .cooling_time_ms = 30.0f
};


Params_TypeDef params = {
    .modify_step_optType = 0,
    .modify_step_value = 0,
    .modify_cooling_time_optType = 0,
    .modify_cooling_time_value = 0
};
