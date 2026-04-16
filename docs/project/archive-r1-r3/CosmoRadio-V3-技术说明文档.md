# Cosmo Radio V3 — 技术说明文档

> 本文档面向技术人员，说明 Cosmo Radio 硬件控制器的技术架构、固件方案、接线方式、使用指南及常见问题。

---

## 一、系统架构

Cosmo Radio 采用安卓平板 + 外置 HID 控制器的分体架构。安卓端运行应用程序，控制器通过 USB HID 协议模拟标准键盘输入，实现物理旋钮和按钮对应用的操控。

```
Android 平板（运行应用）
    ↑ USB Type-C
USB 拓展坞（支持充电 + 数据）
    ↑ USB
ESP32-S3 SuperMini（USB HID 控制器）
    ↑ GPIO
EC11 旋钮 ×2 + 按钮 ×2 + WS2812B LED ×1
```

**核心原理**：ESP32-S3 通过 TinyUSB 协议栈将自身注册为 USB HID 键盘设备，将旋钮旋转和按钮按压转换为标准键盘按键事件，安卓系统无需特殊驱动即可识别。

---

## 二、硬件选型

### 主控：ESP32-S3 SuperMini

| 参数 | 规格 |
|------|------|
| 芯片 | ESP32-S3 |
| Flash | 4MB |
| 接口 | USB OTG（原生 USB，非串口转换） |
| 无线 | WiFi + BLE 5.0（USB 方案中未使用） |
| 尺寸 | 约 22×18mm |
| 供电 | USB 5V |

> 选型关键：ESP32-S3 具备原生 USB OTG 接口（GPIO19/20），可直接运行 TinyUSB 协议栈实现 USB HID，无需外部 USB 转换芯片。

### 输入组件

| 组件 | 规格 | 数量 | 用途 |
|------|------|:----:|------|
| EC11 旋转编码器 | 半月柄 15mm，带开关 | 2 | 频率调节 / 模式切换 |
| 轻触按键开关 | 12×12×7.3mm | 2 | 确认操作（冗余双键） |
| WS2812B RGB LED | 5050 全彩 | 1 | 状态指示 |

### 连接组件

| 组件 | 规格 | 用途 |
|------|------|------|
| Type-C 一分二数据线 | 充电+数据同传 | 连接控制器与平板，并提供充电口 |
| FPC 弯头 Type-C 软排线 | 90° 弯头 | 平板侧接入（节省空间） |

---

## 三、GPIO 引脚分配

### ESP32-S3 SuperMini 引脚映射

| GPIO | 功能 | 说明 |
|:----:|------|------|
| 1 | 按钮 A | 冗余双键，任一触发即生效 |
| 2 | 按钮 B | 冗余双键，任一触发即生效 |
| 3 | 旋钮1 CLK | EC11 编码器1 A相 |
| 4 | 旋钮1 DT | EC11 编码器1 B相 |
| 5 | 旋钮2 CLK | EC11 编码器2 A相 |
| 6 | 旋钮2 DT | EC11 编码器2 B相 |
| 7 | WS2812 LED | RGB LED 数据线 |
| 19 | USB D- | TinyUSB 原生 USB（无需接线） |
| 20 | USB D+ | TinyUSB 原生 USB（无需接线） |

**接线说明**：
- 所有输入 GPIO 使用内部上拉电阻，低电平有效（触发时接 GND）
- EC11 编码器 C 脚（公共端）接 GND
- 按钮另一端接 GND
- WS2812B 数据线接 GPIO7，VCC 接 3.3V，GND 接地

---

## 四、HID 键盘映射

控制器将物理输入映射为标准键盘按键：

| 物理输入 | 键盘按键 | 应用功能 |
|----------|----------|----------|
| 按钮按下 | `Enter` 键按下 | 确认选择 |
| 按钮松开 | `Enter` 键释放 | — |
| 旋钮1 顺时针 | `↑` 上方向键（脉冲） | 频率增加 |
| 旋钮1 逆时针 | `↓` 下方向键（脉冲） | 频率减少 |
| 旋钮2 顺时针 | `→` 右方向键（脉冲） | 下一模式 |
| 旋钮2 逆时针 | `←` 左方向键（脉冲） | 上一模式 |

**按键脉冲机制**：旋钮每转一格，控制器发送一次 20ms 的按键脉冲（按下→延时→释放）。按钮则为持续按住模式（按下保持，松开释放）。

> **安卓端开发注意**：应用应监听标准 `KeyEvent`，对应 `KEYCODE_DPAD_UP/DOWN/LEFT/RIGHT` 和 `KEYCODE_ENTER`。

---

## 五、LED 状态指示

| 事件 | LED 颜色 | 说明 |
|------|----------|------|
| 启动完成 | 绿色闪烁 | 固件启动成功 |
| USB 连接 | 蓝色闪烁 | 已被安卓设备识别 |
| 按钮按下 | 红色常亮 | 视觉反馈 |
| 按钮释放 | 熄灭 | — |

---

## 六、固件方案

### 当前方案：USB HID（推荐）

| 项目 | 说明 |
|------|------|
| 固件目录 | `tusb_hid/` |
| 协议栈 | TinyUSB（Espressif 维护版本） |
| 开发框架 | ESP-IDF v5.x |
| 目标芯片 | ESP32-S3 |
| 连接方式 | USB 有线，即插即用 |
| 延迟 | < 10ms |

**固件代码结构**：

```
tusb_hid/
├── main/
│   ├── tusb_hid_example_main.c   # 主程序：USB 初始化 + 事件分发
│   ├── input_handler.c/h         # GPIO 中断驱动输入处理
│   └── led_indicator.c/h         # WS2812 LED 控制
├── sdkconfig.defaults            # ESP-IDF 配置
└── CMakeLists.txt
```

**USB 设备描述符**：
- 制造商：Cosmo
- 产品名：Pager Radio Input
- 设备类型：HID Keyboard

### 历史方案：BLE HID

| 项目 | 说明 |
|------|------|
| 固件目录 | `ble_hid/` |
| 协议栈 | NimBLE |
| 连接方式 | 蓝牙低功耗无线 |
| 状态 | 已弃用，因蓝牙连接稳定性不足 |

BLE 方案的 `input_handler` 和 `led_indicator` 模块与 USB 方案代码一致，仅通信层不同。

---

## 七、固件构建与烧录

### 环境要求

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- Python 3.8+
- USB 数据线（连接 ESP32-S3 SuperMini）

### 构建步骤

```bash
# 1. 激活 ESP-IDF 环境
get_idf    # 或 source ~/esp/esp-idf/export.sh

# 2. 进入固件目录
cd tusb_hid

# 3. 设置目标芯片（首次构建）
idf.py set-target esp32s3

# 4. 构建
idf.py build

# 5. 烧录（将 ESP32-S3 通过 USB 连接电脑）
idf.py -p /dev/cu.usbmodem* flash

# 6. 查看串口日志（调试用）
idf.py -p /dev/cu.usbmodem* monitor

# 一键构建+烧录+监控
idf.py -p /dev/cu.usbmodem* flash monitor
```

### 烧录注意事项

- ESP32-S3 SuperMini 烧录时需通过其 USB-C 口直连电脑
- 某些 SuperMini 克隆板首次烧录需手动进入下载模式：按住 BOOT 按钮，按一下 RST，松开 BOOT
- 烧录完成后拔出 USB，将 ESP32 接入 USB 拓展坞使用

---

## 八、使用指南

### 首次使用

1. 将 ESP32-S3 SuperMini 通过 USB 连接至 USB 拓展坞
2. 将拓展坞通过 Type-C 线连接至安卓平板
3. 观察 LED：绿色闪烁（启动）→ 蓝色闪烁（USB 连接成功）
4. 打开安卓应用，旋转旋钮和按下按钮测试输入是否生效

### 输入测试

固件附带测试页面 `test/hid-test.html`，可传输至平板用浏览器打开：
- 左侧转盘：响应上下方向键（旋钮1）
- 右侧转盘：响应左右方向键（旋钮2）
- 中心按钮：响应 Enter 键

### 强制重启

长按按钮 **15 秒**触发 ESP32 软重启，用于异常状态恢复。

---

## 九、常见问题

### Q1：插入 USB 后安卓没有反应

- 确认 USB 拓展坞支持数据传输（部分仅支持充电）
- 检查 Type-C 线是否为数据线（非纯充电线）
- 观察 ESP32 LED 是否有蓝色闪烁（表示 USB 已识别）
- 尝试更换 USB 口或重新插拔

### Q2：旋钮转动没有响应

- 检查 EC11 编码器焊接是否牢固（CLK、DT、GND 三个引脚）
- 确认编码器 C 脚（公共端）已接 GND
- 使用串口监控（`idf.py monitor`）查看是否有 `ENC1 CW/CCW` 日志输出

### Q3：按钮按下没有响应

- 检查按钮两端是否分别连接 GPIO（1或2）和 GND
- 按钮使用冗余双键设计，GPIO1 或 GPIO2 任一接低电平即触发
- 串口日志应显示 `BTN -> ENTER (pressed/released)`

### Q4：LED 不亮

- 确认 WS2812B 数据线接 GPIO7（非其他引脚）
- 检查 WS2812B 供电（VCC 接 3.3V）
- 注意 WS2812B 方向性：DIN 为输入端

### Q5：需要修改按键映射

修改 `tusb_hid/main/tusb_hid_example_main.c` 中的 `on_input_event` 函数：

```c
case INPUT_EVENT_ENC1_CW:
    send_key_pulse(KEY_UP_ARROW);    // 修改此处的 keycode
    break;
```

HID keycode 参考：[USB HID Usage Tables](https://usb.org/document-library/hid-usage-tables-14)

### Q6：如何区分 BLE 和 USB 版本控制器

| | BLE 版本 | USB 版本 |
|---|---------|---------|
| 主控 | XIAO ESP32S3 | ESP32-S3 SuperMini |
| 连接 | 蓝牙无线 | USB 有线 |
| 供电 | 锂电池/USB | USB 供电 |
| LED GPIO | GPIO6 | GPIO7 |

---

## 十、技术规格汇总

| 项目 | 规格 |
|------|------|
| 主控芯片 | ESP32-S3（4MB Flash） |
| 通信协议 | USB HID 1.1（键盘） |
| 开发框架 | ESP-IDF v5.x + TinyUSB |
| 输入设备 | EC11 旋转编码器 ×2 + 轻触按键 ×2 |
| 输出设备 | WS2812B RGB LED ×1 |
| 供电方式 | USB 5V（通过拓展坞供电） |
| HID 映射 | Enter / ↑↓←→ 方向键 |
| 按键响应延迟 | < 10ms |
| 旋钮脉冲时长 | 20ms |
| USB 轮询间隔 | 10ms |
| 特殊功能 | 15 秒长按强制重启 |
