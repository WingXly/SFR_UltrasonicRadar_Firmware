#ifndef __STATE_T_H
#define __STATE_T_H


#define STATE_NUM 6

typedef enum {
  INIT         = 0, // 初始化
  PAUSE_APPEND = 1, // 暂停与事件处理
  MEASURING    = 2, // 超声波测距仪正在测距
  PROCESSING   = 3, // 计算距离与处理OLED显示
  ROTATING     = 4, // 发布舵机旋转指令
  COOLING      = 5  // 冷却
} States_TypeDef;

typedef States_TypeDef StateFunction_TypeDef;
typedef StateFunction_TypeDef (*StateMachine_TypeDef)(void);

extern StateFunction_TypeDef State_Init(void);
extern StateFunction_TypeDef State_Pause_append(void);
extern StateFunction_TypeDef State_Measuring(void);
extern StateFunction_TypeDef State_Processing(void);
extern StateFunction_TypeDef State_Rotating(void);
extern StateFunction_TypeDef State_Cooling(void);

// 函数指针表：下标就是状态枚举值
const StateMachine_TypeDef state_machine[STATE_NUM] = {
    [0] = State_Init,
    [1] = State_Pause_append,
    [2] = State_Measuring,
    [3] = State_Processing,
    [4] = State_Rotating,
    [5] = State_Cooling
};

void StateMachine_Run__dont_use_this();


#endif /*__STATE_T_H*/