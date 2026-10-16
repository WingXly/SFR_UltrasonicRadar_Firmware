#include "state_t.h"

const StateMachine_TypeDef state_machine[STATE_NUM] = {
    [0] = State_Init,
    [1] = State_Pause_append,
    [2] = State_Measuring,
    [3] = State_Processing,
    [4] = State_Rotating,
    [5] = State_Cooling
};

#include <stdlib.h>


void StateMachine_Run__dont_use_this(void) {
    static StateFunction_TypeDef next_state = INIT;
    for(;;) {
        StateMachine_TypeDef handler = state_machine[next_state];
        if (handler != NULL) {
            next_state = handler();  // 每个函数返回下一个状态
        } else {
            next_state = INIT;
        }
    }
}