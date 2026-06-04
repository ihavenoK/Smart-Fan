# Smart Fan — 智能风扇物联网系统

基于 **STM32F103C8T6 + FreeRTOS** 的远程控制智能风扇系统，集成 Android App 与 OneNET 云平台实现完整物联网闭环。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        OneNET IoT 云平台                         │
│  ┌─────────┐                           ┌──────────────────┐     │
│  │  MQTT   │◄──── 属性上报/下发 ──────►│  Pulsar 消息流转  │     │
│  │ Broker  │                           │  (设备间属性同步)  │     │
│  └────┬────┘                           └────────┬─────────┘     │
└───────┼──────────────────────────────────────────┼──────────────┘
        │                                          │
    MQTT/TCP                                  Pulsar SSL
        │                                          │
  ┌─────┴──────┐                          ┌───────┴────────┐
  │  ESP8266   │                          │  pulsar/       │
  │  (WiFi)    │                          │  云端桥接服务    │
  └─────┬──────┘                          └────────────────┘
        │ UART (AT指令)
  ┌─────┴──────────────────────────────────┐
  │         STM32F103C8T6 (Cortex-M3)      │
  │         FreeRTOS v10.x                 │
  │                                        │
  │  ┌──────────┐  ┌──────────┐            │
  │  │  DHT11   │  │ EC11编码器│            │
  │  │ 温湿度传感器│  │ 旋钮+按键 │            │
  │  └──────────┘  └──────────┘            │
  │  ┌──────────┐  ┌──────────┐            │
  │  │OLED 0.96"│  │TB6612FNG │            │
  │  │ 128×64   │  │ 电机驱动  │            │
  │  └──────────┘  └──────────┘            │
  └────────────────────────────────────────┘
```

**核心数据流**:
```
Android App ──MQTT──► OneNET ──MQTT──► ESP8266 ──UART──► WiFi Task ──► SystemState
                                                                          │
EC11 ──► Encoder Task ──► Queue ──► Menu Task ──► SystemState ──► Motor Task ──► PWM
                    │                    │                                    │
                    │                    └──► DisplayInfo ──► Display Task ──► OLED
                    │
DHT11 ──► Sensor Task ──► SystemState ──► WiFi Task ──► MQTT 上报
```

---

## 目录结构

```
freertos v1/
├── stm32/                          # STM32 固件工程 (Keil MDK)
│   ├── App/                        # 应用层任务 (6 个 RTOS 任务)
│   │   ├── encoder_task.c          #   EC11 旋转编码器 (Ben Buxton 四倍频状态机)
│   │   ├── wifi_task.c             #   ESP8266 + MQTT 连接管理 (7步状态机)
│   │   ├── motor_task.c            #   电机斜坡加减速控制 (50ms 周期)
│   │   ├── menu_task.c             #   UI 菜单状态机 (5 个页面)
│   │   ├── sensor_task.c           #   DHT11 温湿度采集 (2s 周期)
│   │   ├── display_task.c          #   OLED 显示刷新 (200ms 周期)
│   │   ├── tasks.h                 #   任务接口 + 内核对象声明
│   │   └── system_state.h          #   系统状态数据结构定义
│   ├── Hardware/                   # 硬件驱动层
│   │   ├── dht11.c/h               #   DHT11 温湿度传感器 (DWT 微秒延时)
│   │   ├── ec11.c/h                #   EC11 编码器 (EXTI 中断 + 消抖)
│   │   ├── motor.c/h               #   TB6612FNG 电机 (10kHz PWM)
│   │   ├── OLED.c/h/OLED_Data.c/h  #   OLED 128×64 I²C 驱动
│   │   ├── esp8266.c/h             #   ESP8266 USART + AT 指令
│   │   ├── delay.c/h               #   DWT 周期计数器微秒延时
│   │   ├── debug_serial.c/h        #   调试串口 (USART1)
│   │   ├── Serial.c/h              #   USART 基础封装
│   │   └── LED.c/h                 #   LED 指示灯
│   ├── FreeRTOS/                   # FreeRTOS 内核 (v10.x)
│   │   ├── src/                    #   内核源码 (tasks, queue, timers, event_groups...)
│   │   ├── inc/                    #   头文件
│   │   └── port/                   #   Cortex-M3 移植层 (heap_4, FreeRTOSConfig.h)
│   ├── Library/                    # STM32 标准外设库 (v3.5)
│   ├── Start/                      # 启动文件 + 系统时钟配置
│   ├── User/                       # 用户代码
│   │   ├── main.c                  #   主入口: 硬件初始化 → 创建任务 → 启动调度器
│   │   ├── stm32f10x_it.c          #   中断服务函数
│   │   ├── stm32f10x_conf.h        #   外设库配置头
│   │   ├── FreeRTOS_Hooks.c        #   断言回调 + 栈溢出钩子
│   │   └── system_config.h         #   全局宏配置
│   ├── project.uvprojx             # Keil 工程文件
│   └── keilkill.bat                # 清理编译产物
├── android/                        # Android 控制端 App
│   └── app/src/main/java/com/example/esp8266_app/
│       ├── MainActivity.java       #   主界面 (模式/风速/定时/温湿度显示)
│       ├── MqttHelper.java         #   MQTT 连接封装 (Eclipse Paho)
│       └── TokenUtil.java          #   OneNET 认证 Token (HMAC-SHA256)
├── pulsar/                         # OneNET Pulsar SDK 桥接服务
│   └── src/main/java/com/chinamobile/iot/
│       ├── IoTPulsarConsume.java   #   主入口 (设备属性同步)
│       ├── IoTConsumer.java        #   Pulsar Consumer 封装
│       └── AESBase64Utils.java     #   AES 消息解密
└── README.md
```

---

## 硬件规格

### 主控
| 参数 | 规格 |
|------|------|
| MCU | STM32F103C8T6 (Cortex-M3, 72MHz) |
| Flash / RAM | 64KB / 20KB |
| RTOS | FreeRTOS v10.x, 动态分配 (heap_4), 15KB 堆 |

### 外设引脚分配

| 外设 | 引脚 | 接口 | 说明 |
|------|------|------|------|
| DHT11 | PA0 | GPIO (I/O 双向) | 温湿度传感器，单总线协议 |
| OLED | PB6(SCL), PB7(SDA) | I²C | 0.96 寸 128×64 蓝光 OLED |
| EC11 A相 | PA7 | GPIO (EXTI Line7, 下降沿) | 旋转编码器 A 相 |
| EC11 B相 | PA6 | GPIO (EXTI Line6, 下降沿) | 旋转编码器 B 相 |
| EC11 按键 | PA5 | GPIO (EXTI Line5, 双边沿) | 编码器按键 (上拉输入) |
| TB6612 STBY | PB12 | GPIO (推挽输出) | 电机待机控制 |
| TB6612 AIN1 | PB13 | GPIO (推挽输出) | A 通道方向 1 |
| TB6612 AIN2 | PB14 | GPIO (推挽输出) | A 通道方向 2 |
| TB6612 PWM | PA1 | TIM2_CH2 (AF) | 10kHz PWM 调速 |
| ESP8266 TX | PA2 | USART2 TX | Wi-Fi 模块发送 |
| ESP8266 RX | PA3 | USART2 RX | Wi-Fi 模块接收 |
| Debug TX | PA9 | USART1 TX | 调试串口 (9600bps) |
| Debug RX | PA10 | USART1 RX | 调试串口 |

---

## FreeRTOS 任务设计

### 优先级分配与内存

| 任务 | 优先级 | 栈大小 | 周期 | 核心职责 |
|------|--------|--------|------|----------|
| **Encoder** | 6 (最高) | 256B | 事件驱动 (5ms 保底) | EC11 消抖 + 方向解码 → 推入 Queue |
| **WiFi** | 5 | 768B | 事件驱动 | ESP8266 初始化 + MQTT 收发 + 状态上报 |
| **Motor** | 3 | 256B | 精确 50ms | 斜坡加减速 + PWM 更新 |
| **Menu** | 2 | 512B | 事件驱动 (1s 超时) | 编码器事件消费 → 菜单状态机 → 更新显示缓冲 |
| **Sensor** | 1 | 256B | 2000ms | DHT11 采集 → 更新 SystemState |
| **Display** | 1 | 512B | 200ms | 读取 DisplayInfo → OLED 渲染 |

### 任务间通信

```
Encoder ──Queue(4)──► Menu ──DisplayInfo──► Display ──OLED
                  │
Sensor ──Queue(2)──┘
                  │
          SystemState ←── Mutex 保护 ──→ Motor / WiFi / Menu
```

- **Encoder Queue**: 容量 4，`EncoderMsg` 类型，传递旋转/按键事件
- **Sensor Queue**: 容量 2，`SensorData_t` 类型，传递温湿度数据 (当前未消费)
- **SystemMutex**: 保护全局 `g_SystemState` 结构体
- **OledMutex**: 保护 OLED 总线访问 (I²C 非重入)
- **任务通知**: Encoder ISR → Encoder Task / USART2 ISR → WiFi Task

### FreeRTOS 关键配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| CPU_CLOCK | 72MHz | 系统主频 |
| TICK_RATE | 1000Hz | 1ms 滴答周期 |
| MAX_PRIORITIES | 8 | 任务优先级范围 0-7 |
| MINIMAL_STACK | 128 字 (512B) | 最小任务栈 |
| TOTAL_HEAP | 15KB | heap_4 总堆大小 |
| MAX_SYSCALL_INT_PRIORITY | 5 (80) | NVIC 优先级分组 4 |
| KERNEL_INT_PRIORITY | 15 (240) | 最低中断优先级 |
| CHECK_FOR_STACK_OVERFLOW | 2 | 全面检测模式 |
| USE_PREEMPTION | 1 | 抢占式调度 |
| USE_TASK_NOTIFICATIONS | 1 | 启用任务通知 |

---

## 功能详解

### 1. 三种运行模式

| 模式 | 触发方式 | 行为 |
|------|----------|------|
| **手动 (MANUAL)** | 默认模式 / 菜单选择 | 旋转编码器实时调速 (步进 5%，0-100%) |
| **自动 (AUTO)** | 菜单选择 | 根据 DHT11 温度自动调速 (≤24°C 关闭, 24-28°C 中速, 28-32°C 高速, ≥32°C 全速) |
| **定时 (TIMER)** | 菜单设置倒计时 | 手动调速，倒计时归零后自动关闭 |

### 2. EC11 编码器操作

| 操作 | 主页 | 菜单中 |
|------|------|--------|
| 旋转 CW/CCW | 手动模式下调速 (±5%) | 导航/调节参数 |
| 短按 | 无操作 | 确认/选择 |
| 长按 (3s) | 进入主菜单 | 返回主页 |

### 3. 电机控制

- **驱动芯片**: TB6612FNG
- **调速方式**: TIM2_CH2 10kHz PWM (占空比 0-100%，分辨率 1%)
- **斜坡算法**: 每 50ms 步进 ±1%，实现平滑启停
- **保护机制**: 紧急停机 (拉低 STBY) / 惯性刹车 (短路制动)

### 4. MQTT 通信

```
Topic 发布: $sys/{product_id}/{device}/thing/property/post     (属性上报)
Topic 订阅: $sys/{product_id}/{device}/thing/property/set       (属性下发)
Topic 回复: $sys/{product_id}/{device}/thing/property/set_reply (下发确认)
```

上报字段: `speed`, `temp`, `humi`, `mode`, `countdown`

上报策略:
- 手动触发: 风速变化 → 即时上报风速
- 定期上报: 每 60 秒全量上报
- 云下发: Android 端操作 → 实时同步到 STM32

---

## OneNET 云平台配置

### 产品信息 (硬编码)

| 参数 | 值 |
|------|-----|
| Product ID | `e2F2Jz2iUU` |
| 设备名 (STM32) | `esp` |
| 设备名 (App) | `app` |
| MQTT Broker | `mqtts.heclouds.com:1883` |
| 认证方式 | MD5 Token / HMAC-SHA256 |

### Wi-Fi 凭证 (需修改)

`stm32/App/wifi_task.c` 中的硬编码配置:

```c
#define WIFI_SSID       "ihavenok"       // ← 修改为你的 Wi-Fi SSID
#define WIFI_PASS       "88888888"       // ← 修改为你的 Wi-Fi 密码
#define MQTT_TOKEN      "version=..."    // ← 修改为 OneNET 生成的设备 Token
```

---

## 开发环境

### STM32 固件

| 工具 | 版本/说明 |
|------|-----------|
| IDE | Keil MDK-ARM v5 |
| 编译器 | ARMCC v5 (AC5) |
| 标准外设库 | STM32F10x SPL v3.5 |
| FreeRTOS | v10.x (内嵌源码) |
| 调试器 | ST-Link / J-Link (SWD) |

编译步骤:
1. 打开 `stm32/project.uvprojx`
2. 修改 `wifi_task.c` 中的 Wi-Fi SSID/密码/Token
3. 编译 (F7) → 下载 (F8)

### Android App

| 工具 | 版本/说明 |
|------|-----------|
| IDE | Android Studio |
| 最低 SDK | API 21 (Android 5.0) |
| MQTT 库 | Eclipse Paho v1.2.5 |

### Pulsar 桥接服务

| 工具 | 版本 |
|------|------|
| JDK | 1.8+ |
| Maven | 3.6+ |

---

## 启动流程

```
上电 → NVIC 优先级分组 → 硬件初始化
  ├─ DWT 延时校准
  ├─ Debug 串口 (USART1, 9600)
  ├─ OLED 初始化 (显示 "Smart Fan Starting...")
  ├─ DHT11 初始化
  ├─ EC11 初始化 + EXTI 中断配置
  └─ 电机初始化 (TB6612FNG)
     ↓
创建内核对象 (Queue x2, Mutex x2)
     ↓
创建 6 个任务 (Encoder→WiFi→Motor→Menu→Sensor→Display)
     ↓
vTaskStartScheduler() 启动调度器
     ↓
WiFi 任务 7 步初始化:
  [1] AT+RST          → 模块复位
  [2] AT+CWMODE=1     → STA 模式
  [3] AT+CWJAP=...    → 连接 AP (12s 超时)
  [4] AT+CIPMUX=0     → 单连接
  [5] AT+MQTTUSERCFG  → MQTT 用户配置
  [6] AT+MQTTCONN     → 连接 Broker (15s 超时)
  [7] AT+MQTTSUB      → 订阅下发 Topic
     ↓
进入 WIFI_IDLE 主循环
```

---

## 代码审查记录 (Embedded Review)

> 基于 embedded-review skill 的 P0-P3 严重度分级标准。

### 🟡 P2 — 建议修复

#### 1. ESP8266 接收缓冲区竞态 (esp8266.c:14, 139-149)
- **问题**: `rx_buf` 和 `rx_len` 被 ISR 并发写入，而 `ESP8266_CheckResponse()` 在任务上下文中通过 `strstr()` 读取。`strstr` 依赖 null 终止符，但 ISR 可能在扫描过程中追加数据，导致 `strstr` 读取过界或错过匹配。
- **风险**: 偶发性 MQTT 消息丢失或 AT 命令响应误判。
- **修复建议**: 在 `ESP8266_CheckResponse()` 中先关 RXNE 中断 (或复制快照) 再扫描；或改用定长窗口扫描而非 `strstr`。

#### 2. DHT11 临界区过长 (dht11.c:112-134)
- **问题**: `taskENTER_CRITICAL()` 在 DHT11 读取期间持续约 5ms (40 bit × ~125μs/bit)，期间屏蔽所有中断。对 1ms tick 的 RTOS 而言 5ms 的关中断是较重的。
- **风险**: 1kHz 滴答下丢失最多 5 个 tick，可能影响其他任务的时间精度。
- **缓解**: 当前已通过在临界区前用 `vTaskDelay(20ms)` 释放 CPU 来减轻影响。如需进一步优化可考虑使用定时器捕获替代 bit-banging。

#### 3. WiFi 故障无恢复机制 (wifi_task.c:289)
- **问题**: `WIFI_FAILED` 状态下任务调用 `vTaskSuspend(NULL)` 永久挂起，WiFi 一旦初始化失败永不重试。
- **风险**: 需要手动复位设备才能恢复连接。
- **修复建议**: 增加定时重试逻辑 (如每 60 秒重置状态机到 `WIFI_INIT_START`)。

#### 4. MQTT Token 硬编码且含过期时间 (wifi_task.c:22)
- **问题**: Token 中 `et=1811086080` 表示过期时间戳，到期后 MQTT 连接将失败。
- **风险**: Token 过期后设备无法连接 OneNET。
- **修复建议**: 实现 AT+MQTTTOKEN 动态刷新，或将 Token 移至外部配置区方便更新。

### ⚪ P3 — 可选改进

#### 5. PWM 频率宏与实际不一致 (system_config.h:15 vs motor.c:40-43)
- **现象**: `system_config.h` 定义 `FAN_PWM_FREQ 25000`，但 `motor.c` 实际配置为 10kHz (72M/72/100)。
- **建议**: 统一为 10kHz 或删除误导性宏定义。

#### 6. Menu UpdateDisplay 无谓刷新 (menu_task.c:54)
- **现象**: 每次定时器超时 (1s) 无论状态是否变化都调用 `Menu_UpdateDisplay()` 重新格式化字符串，即使页面没有变化。
- **建议**: 增加 dirty flag，仅在状态变化时刷新。

#### 7. Sensor Queue 未被消费
- **现象**: `xSensorQueue` 创建并接收数据，但没有任何任务从该队列读取。
- **建议**: 若不需要队列缓冲则移除，或将队列用于自动温控任务。

---

## 已知限制

1. **Wi-Fi 仅支持 2.4GHz**: ESP8266 不支持 5GHz 频段
2. **无 OTA 升级**: 固件更新需通过 SWD 烧录
3. **无数据持久化**: 断电后定时器/模式设置丢失
4. **单风机控制**: 仅有一个 TB6612FNG A 通道，不支持多风扇
5. **DHT11 精度有限**: 温度 ±2°C，湿度 ±5%RH；如需更高精度可替换为 DHT22/SHT30

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0.0 | 2025 | 初始版本: 6 任务架构, 手动/自动/定时三模式, MQTT 远程控制 |

---

## 许可证

教育/个人项目，无特定许可证。
