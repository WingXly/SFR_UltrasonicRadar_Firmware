#include "state_t.h"
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