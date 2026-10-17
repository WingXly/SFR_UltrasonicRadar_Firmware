# 基于 FreeRTOS 与 ROS2 的分布式超声波扫描系统（下位机）

注意：此仓库为 STM32 下位机固件

---

## 下位机简介

基于 STM32F103C8T6 芯片、FreeRTOS 系统内核的超声波雷达扫描系统下位机。

---

## 目录结构

```text
firmware/                    # STM32 下位机工程
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/                 # CubeMX生成的驱动
├── Middlewares/             # 中间件，包括 FreeRTOS / CMSIS
├── CMakeLists.txt
└── UltrasonicRadar.ioc      # STM32CubeMX 工程
```

---

## 功能特性

- 上下位机二进制通信协议：帧头、长度、数据、XOR 校验、帧尾。
- 蓝牙 SPP / USB 串口双向通信。
- 基于 FreeRTOS 内核，使用 DMA + USART 空闲中断接收。
- 下位机代码按模块面向对象封装：LED、OLED、定时器、串口、蓝牙。
- 使用函数指针数组管理 MainTask 状态机。
- 支持按键暂停/恢复、LED 状态指示、OLED 开关。
- HC-SR04 + SG90 实现 0~180° 扫描。

---

## 快速使用

### 编译

```bash
cd firmware
cmake -S . -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
```

生成的固件：

```text
firmware/build/Debug/UltrasonicRadar.elf
```

### 烧录

使用 STM32CubeProgrammer 或 ST-Link Utility 烧录。

---

## 技术栈（下位机）

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6 |
| 蓝牙 | JDY-31-SPP |
| OLED | CH1116，I2C |
| 舵机 | SG90 |
| 超声波 | HC-SR04 |
| 开发环境 | STM32CubeMX + CMake + arm-none-eabi-gcc + VS Code |
| RTOS | FreeRTOS / CMSIS-RTOS2 |
| 通信 | USART3 + DMA + IDLE 中断 |
| PWM | TIM4 CH3 |
| 输入捕获 | TIM1 CH3/CH4 |
| 烧录 | ST-Link V2 |

---

## 硬件连接

### STM32 引脚分配

| STM32 引脚 | 配置 | 说明 |
|---|---|---|
| PA10  | TIM1_CH3                  | 上升沿捕获 |
| PA11  | HC-SR04 Trig + TIM1_CH4   | 用于触发 HC-SR04，同时作为下降沿捕获 |
| PB8   | SG90 PWM                  | TIM4_CH3，50Hz PWM |
| PB6   | OLED SCL                  | I2C1_SCL |
| PB7   | OLED SDA                  | I2C1_SDA |
| PB10  | 蓝牙 RX / USART3_TX       | 接蓝牙模块 RX |
| PB11  | 蓝牙 TX / USART3_RX       | 接蓝牙模块 TX |
| PA6   | 蓝色 LED                  | 控制蓝色LED小灯亮灭 |
| PA7   | 绿色 LED                  | 控制绿色LED小灯亮灭 |
| PB0   | 红色 LED                  | 控制红色LED小灯亮灭 |
| PB12  | KEY1                      | KEY1（上拉）用于控制主程序暂停/恢复 |
| PB13  | KEY2                      | KEY2（上拉）用于控制OLED 开关 |

> 蓝牙模块与 STM32 必须共地，TX/RX 交叉连接。

> 舵机与 STM32 必须共地，使用 5V 供电。

---

## 更多技术细节

### FreeRTOS 任务

| 任务 | 优先级 | 职责 |
|---|---|---|
| `Task_Main` | Normal1 | 状态机递进、超声波测距、舵机控制 |
| `Task_BluetoothIO` | High | 蓝牙收发、协议解析 |
| `Task_OledUpdate` | Low1 | OLED 刷新、清屏、开关处理 |

### 消息队列

| 队列 | 长度 | 元素 | 用途 |
|---|---:|---|---|
| `queue_btSendData_` | 8 | `float` | 需要通过蓝牙进行发送的信息，包括传感器数据或错误信息等 |
| `queue_btRxCommand_` | 16 | `CommandCode_TypeDef*` | 蓝牙收到的上位机命令（需要先经过 Cmd_Parse() 解析再传入） |

其中， `CommandCode_TypeDef` 是本项目用于描述一段指令文本的结构体类型：

```c
/*指令类型表*/
typedef enum {
    CMD_PAUSE_OR_START = 0,
    CMD_OLED           = 1,
    CMD_SPEED          = 2,
    CMD_COOLING_TIME   = 3,
    CMD_RESAMPLE       = 4
} CommandType_TypeDef;

typedef struct {
    CommandType_TypeDef type;   // 命令类型
    uint16_t argc;              // 参数个数
    uint16_t argv[CMD_ARG_MAX]; // 参数列表
} CommandCode_TypeDef;
```

消息流：

```text
Task_Main -- 传感器数据/错误码 --> queue_btSendData_ --> Task_BluetoothIO
Task_BluetoothIO -- 解析 --> 解析后的命令文本 -- queue_btRxCommand_ --> Task_Main
```

### 定时器

| 定时器 | 作用 |
|---|---|
| TIM1 | HC-SR04 输入捕获。PA10 -> CH3 上升沿，CH4 间接捕获下降沿 |
| TIM3 | 主循环计时。HC-SR04 触发 10us 计时、冷却时间计时 |
| TIM4 | 舵机 PWM，CH3 -> PB8，20ms 周期 |

### 状态机

状态转移：

```text
INIT
  -> PAUSE_APPEND
      -> MEASURING
          -> PROCESSING
              -> ROTATING
                  -> COOLING
                      -> PAUSE_APPEND
```

无事件表。

---

## 二进制通信协议

### 1. 传感器数据帧

下位机 -> 上位机，长度 9 字节：

```text
数组类型：uint8_t
0      1      2       3       4      5       6      7      8
0XAA   0X55   长度N   角度H   角度L   距离H   距离L   校验   0X0D
```

| 字段 | 说明 |
|---|---|
| 帧头 | `0xAA 0x55` |
| 长度 N | `5` |
| 角度 | `int16`，单位 0.01° |
| 距离 | `uint16`，单位 mm |
| 校验 | XOR校验和，即 `长度N ^ 角度H ^ 角度L ^ 距离H ^ 距离L` |
| 帧尾 | `0x0D` |

### 2. 单参数命令帧

上位机 -> 下位机，长度 9 字节：

```text
AA 55 05 [cmd_type_h] [cmd_type_l] [param_h] [param_l] [checksum] 0D
```

### 3. 双参数命令帧

上位机 -> 下位机，长度 11 字节：

```text
AA 55 07 [cmd_type_h] [cmd_type_l] [p1_h] [p1_l] [p2_h] [p2_l] [checksum] 0D
```

> `cmd_type`、参数的数据类型均为大端 `uint16_t`。

---

## 指令表

- CLI指令

| 文本指令 | 机器码 | 作用 |
|---|---|---|
| `start` | `0 0` | 解除暂停 |
| `stop` | `0 1` | 暂停 |
| `oled on` | `1 1` | 打开 OLED |
| `oled off` | `1 0` | 关闭 OLED |
| `speed set N` | `2 1 N` | 设置舵机速度 |
| `speed add N` | `2 2 N` | 增加舵机速度 |
| `speed remove N` | `2 3 N` | 减少舵机速度 |
| `cooling_time set N` | `3 1 N` | 设置冷却时间 |
| `cooling_time add N` | `3 2 N` | 增加冷却时间 |
| `cooling_time remove N` | `3 3 N` | 减少冷却时间 |
| `resample N` | `4 N` | 设置重复采样次数 |

- 指令文本

```text
0 0        # 解除暂停
0 1        # 暂停
1 0        # 关闭 OLED
1 1        # 打开 OLED
2 1 X      # 设置舵机速度
3 1 X      # 设置冷却时间
4 X        # 设置重复采样次数
```

> X 必须是正数

### 错误码

| 错误码 | 含义 |
|---|---|
| `0x00` | 参数无效 |
| `0x01` | 参数个数与指令不匹配 |
| `0x02` | 未找到匹配的指令 |

---

## 常见问题

### 1. 舵机不转

- 确认 `pause == false`；
- 检查 PB8 是否接舵机信号线；
- 确认 TIM4 PWM 周期为 20ms；
- 检查舵机以 5V 供电；
- 确认舵机与 STM32 是否共地。

### 2. 超声波不测距

- 检查 PA11 Trig、PA10 Echo 接线；
- 触发高电平需要至少 10us；
- 检查 TIM1 捕获中断和 TIM3 计时；
- 注意 Echo 电平，必要时做电平转换。

### 3. OLED 不亮

- 检查 I2C 地址，常见 `0x78` / `0x7A`；
- 在 CubeMX 中检查 I2C1 所用引脚为 PB6/PB7（上拉）；
- 确认 CH1116 与 SSD1306 初始化命令差异。

---

## 许可证

本仓库包含以下不同许可的部分：

- **STM32CubeMX 生成代码（`Core/`、`Drivers/`、`Middlewares/` 等）**：
  版权归 STMicroelectronics 所有，适用 BSD-3-Clause 许可证。
  详见各文件头部注释。

- **项目自有代码（`Core/Inc`、`Core/Src` 的用户自定义部分）**：
  适用 MIT 许可证。详见 [LICENSE](LICENSE)。