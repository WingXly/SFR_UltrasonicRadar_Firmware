#ifndef __STATE_T_H
#define __STATE_T_H

#include <stdint.h>


/*状态表*/
typedef enum {
  INIT         = 0, // 初始化
  PAUSE_APPEND = 1, // 暂停与事件处理
  MEASURING    = 2, // 超声波测距仪正在测距
  PROCESSING   = 3, // 计算距离与处理OLED显示
  ROTATING     = 4, // 发布舵机旋转指令
  COOLING      = 5  // 冷却
} State_TypeDef;
typedef State_TypeDef (*StateHandler_TypeDef)(void);


/*定义在 freeetos.c*/
State_TypeDef State_Init(void);
State_TypeDef State_Pause_append(void);
State_TypeDef State_Measuring(void);
State_TypeDef State_Processing(void);
State_TypeDef State_Rotating(void);
State_TypeDef State_Cooling(void);


State_TypeDef StateMachine_Next(State_TypeDef this_state);


#endif /* __STATE_T_H */
