# USB HID 固件实现

> ⚠️ **2026-05-18 状态**：本文件原始内容（V2 XIAO ESP32S3 时代 GPIO 1/D0 描述）已过时，整理为下方简明版。
> 完整 V4 实现细节见 [`/CLAUDE.md`](../../CLAUDE.md) "HID Key Mappings" + "GPIO Pin Assignments"。

## 概述

基于 **TinyUSB** 的 USB HID 键盘实现，运行在 **ESP32-S3 DevKitC N16R8**。把物理输入（双 EC11 旋钮 + Kailh BOX 按钮 + RC522 NFC）转换为键盘事件。USB OTG 通过外置 dongle (YK16-09E V1) 实现"平板 USB Host + 充电"同时工作。

V1/V2 BLE HID 方案已删除（见 git 历史）；V3 SuperMini → V4 DevKitC N16R8 已完成迁移。

## 当前固件入口

| 文件 | 职责 |
|------|------|
| `main/tusb_hid_example_main.c` | TinyUSB 初始化 + ASCII→HID keycode 编码 + 多键并发报告管理（`s_pressed_keys[6]` + `s_hid_mutex`）|
| `main/input_handler.c/h` | GPIO 中断驱动状态机：Action Button + 双 EC11 (A/B/SW)，事件队列分发 |
| `main/nfc_handler.c/h` | RC522 SPI (1MHz, SPI2 via GPIO Matrix) + NDEF Text Record 解析 + 1.5s 同卡去重 |
| `main/led_indicator.c/h` | DevKitC GPIO48 板载 WS2812B RGB 状态指示 |

## HID 输入映射

权威表见 [/CLAUDE.md](../../CLAUDE.md) "HID Key Mappings"，简要：

| 输入 | HID 按键 |
|------|---------|
| Action Button 按 | Enter |
| EC11-L 旋转 / 按 | ↑/↓ / F1 |
| EC11-R 旋转 / 按 | →/← / F2 |
| NFC 卡 (NDEF Text) | `<payload>\n` |
| NFC 卡 (UID 兜底) | `NFC:<UID_HEX>\n` |

> GPIO 号 / 连接器映射见 CLAUDE.md "GPIO Pin Assignments"，本文不重复。

## LED 行为

DevKitC GPIO48 板载 RGB（不外接 LED）：

| 事件 | 颜色 |
|------|------|
| 启动完成 | 绿色闪烁 |
| 按钮按下 | 红色 |
| NFC 识别中 | 蓝色 |

## 构建与烧录

```bash
get_idf                                    # 激活 ESP-IDF 环境
idf.py set-target esp32s3                  # 首次
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor  # 走 DevKitC UART USB-C 烧录，不走 OTG USB-C
```

> ⚠️ **烧录走 UART USB-C，不要走 OTG USB-C** — 板上 USB D+/D- 同时连到 J5 (dongle) 和 native USB-C 焊盘，外部插会冲突。详见 CLAUDE.md。

## 已知技术债

详见 [/CLAUDE.md](../../CLAUDE.md) "Known Limitations / Technical Debt"：

- NFC SPI 时钟暂用 1MHz（PCB 版可尝试拉回 5MHz）
- HID 多键并发已修复（2026-05-08 `s_pressed_keys[6]` + mutex）
- NFC 启动失败容错已实现（2026-05-10 warn 不 abort）
- sdkconfig 已升级 N16R8（2026-05-08 16MB QIO + 8MB Octal PSRAM）

## 硬件需求

详见 [/CLAUDE.md](../../CLAUDE.md) "Hardware Target (V4)" + [/docs/project/CosmoRadio-V4-PCB-Spec.md](../project/CosmoRadio-V4-PCB-Spec.md)。
