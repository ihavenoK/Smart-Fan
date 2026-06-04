# Smart Fan — 智能风扇物联网系统

基于 **STM32F103C8T6 + FreeRTOS** 的智能风扇，支持 Android App 远程控制，通过 OneNET 云平台实现设备间通信。

## 项目结构

```
├── stm32/      # 固件工程（Keil MDK）
├── android/    # Android 控制端
├── pulsar/     # OneNET Pulsar SDK 云端桥接
└── README.md
```

## 硬件概览

| 组件 | 型号 |
|------|------|
| MCU | STM32F103C8T6 (Cortex-M3, 72MHz) |
| RTOS | FreeRTOS v10.x |
| 传感器 | DHT11 温湿度 |
| 显示 | 0.96" OLED 128×64 (I²C) |
| 输入 | EC11 旋转编码器 |
| 电机 | TB6612FNG 驱动 |
| Wi-Fi | ESP8266 (AT 指令) |

## 功能

- **手动模式**：编码器实时调速 (0-100%)
- **自动模式**：根据温度自动调速
- **定时模式**：倒计时自动关闭
- **远程控制**：Android App 通过 MQTT 下发指令
- **数据上报**：温湿度、风速实时上传 OneNET

## 使用方法

### STM32 固件
1. 用 Keil MDK 打开 `stm32/project.uvprojx`
2. 在 `stm32/App/wifi_task.c` 中修改 Wi-Fi SSID、密码和 OneNET Token
3. 编译烧录到开发板，OLED 显示 "Smart Fan Starting..." 即启动成功

### Android App
1. 用 Android Studio 打开 `android/` 目录
2. 修改 `MainActivity.java` 中的 OneNET Product ID 和 MQTT 连接信息
3. 编译安装到手机，连接同一 OneNET 产品即可控制风扇

### Pulsar 桥接
1. 修改 `pulsar/src/main/java/.../IoTPulsarConsume.java` 中的 productId 和 accessKey
2. 执行 `mvn clean package` 编译打包
3. 运行 jar 包后，自动在两个设备间同步属性

## 编码器操作

| 操作 | 主页 | 菜单中 |
|------|------|--------|
| 旋转 | 手动模式下调节风速 | 导航 / 调节参数 |
| 短按 | — | 确认 |
| 长按 3 秒 | 进入菜单 | 返回主页 |

## 开发环境

| 模块 | IDE / 工具 |
|------|-----------|
| STM32 | Keil MDK v5 |
| Android | Android Studio |
| Pulsar | JDK 1.8+ / Maven 3.6+ |
