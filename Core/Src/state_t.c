#include "state_t.h"
#include "stdlib.h"
#include <stdint.h>


#define STATE_MAP_SIZE (sizeof(states_map) / sizeof(states_map[0]))


/*状态机*/
static const struct {
  State_TypeDef state;
  StateHandler_TypeDef handler;
} states_map[] = {
    ///={       state          ,         handler          },
    {INIT                  , State_Init               },
    {PAUSE_APPEND          , State_Pause_append       },
    {MEASURING             , State_Measuring          },
    {PROCESSING            , State_Processing         },
    {ROTATING              , State_Rotating           },
    {COOLING               , State_Cooling            },
};


State_TypeDef StateMachine_Next(State_TypeDef this_state)
{
    for (uint8_t i = 0; i < STATE_MAP_SIZE; i++) {
        if (states_map[i].state == this_state)
        {
            StateHandler_TypeDef handler = states_map[i].handler;
            State_TypeDef next_state = handler();

            if (next_state) {
                return next_state;
            }
        }
    }

    return INIT;
}
