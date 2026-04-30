# CosmoRadio

可穿梭于平行宇宙的 Radio 接收器

## 项目简介

桌面交互设备，安卓平板 + 3D 打印外壳 + ESP32 USB HID 控制器。造型参考 iMac G3 斜面站立式设计，用户通过旋钮调频、NFC 刷卡切换内容，仿佛在平行宇宙间搜索信号。

**当前版本：V4**（2026.04 — 2026.05）— 技术方案验证 + 单套原型制作

> 2026-04-29 与 Arthur 达成重启协议：第四期缩小为技术方案验证和单套原型制作，合同价 ¥5,000，目标 2026-05-20 左右交付。原 4 套整机 + 8 套电子套件交付范围已取消。

## 产品形态

- **外观风格**: iMac G3 斜面站立式，模块化三件式（主体 + 顶部 + 背板）
- **显示设计**: 三圆形遮罩（矩形屏幕通过开孔分割为三个圆形显示区）
- **输入方式**: 2 × EC11 旋钮 + Kailh 机械轴按钮 (6.25U 卫星轴) + NFC 刷卡
- **状态指示**: WS2812 RGB LED

## 硬件组成（V4）

| 组件 | 规格 | 用途 |
|------|------|------|
| 安卓平板 | 客户自购 | 主机，运行 Web 应用 |
| ESP32-S3 DevKitC N16R8 | 36+ GPIO / USB OTG / 16MB Flash / 8MB PSRAM | HID 控制器 |
| 定制 PCB 载板 | 80×50mm 双面 (JLCPCB) | 消灭飞线，XH2.54 即插即用 |
| EC11 旋钮 × 2 | 15mm 半轴柄旋转编码器 | 频率调节 / 模式切换 |
| Kailh BOX 轴 + 6.25U 卫星轴 | 机械键盘蝴蝶结构，3.6mm 行程 | Action Button |
| RC522 mini NFC 模块 × 1~2 | 13.56MHz SPI，读 NTAG215 UID | NFC 刷卡交互（数量待确认） |
| WS2812B LED × 1 | 5050 RGB | 状态指示 |
| USB-C 大电流转接板 × 2 + SS34 | PCB 板载 VBUS 注入，替代 FPC+一分二 | 充电 + 数据单线直连 |

## 连接方案

| 方案 | 固件位置 | 状态 |
|------|----------|------|
| USB HID | 项目根目录 | **当前使用** — TinyUSB，即插即用 |
| BLE HID | (已删除，见 git 历史) | V1-V2 遗留，已停用 |

## 交互设计

输入设备通过 ESP32 HID 协议（蓝牙或 USB）连接到安卓系统，模拟键盘按键：

| 输入 | 映射 | 功能 |
|------|------|------|
| 顶部按钮 | Enter 键 | 确认选择 |
| EC11 #1 旋转 | 上/下方向键 | 调节频率 |
| EC11 #2 旋转 | 左/右方向键 | 切换模式 |

### LED 状态指示

| 事件 | LED 颜色 |
|------|----------|
| 按钮按下 | 红色 |
| 按钮释放 | 熄灭 |
| 连接成功 | 蓝色闪烁 |
| 启动完成 | 绿色闪烁 |

## 项目结构

```
cosmo-radio/                            # 项目根 = ESP-IDF USB HID 固件
├── main/
│   ├── tusb_hid_example_main.c         # 入口，USB HID + 输入处理
│   ├── input_handler.c/h               # GPIO 中断输入
│   └── led_indicator.c/h               # WS2812 LED 控制
├── sdkconfig.defaults
├── CMakeLists.txt
├── docs/
│   ├── assets/                         # 元器件引脚图、参考图
│   ├── firmware/                       # 固件实现文档
│   ├── hardware/                       # 硬件模块设计方案
│   ├── legacy/                         # 弃用内容
│   └── project/ → codex               # BOM、PCB Spec、项目主文档
└── test/
    └── hid-test.html
```

## 相关项目

- [cosmo-pager-web-nextjs-vercel](https://github.com/RogerHao/cosmo-pager-web-nextjs-vercel) — Web 应用前端（客户软件团队开发）

## 版本历史

| 版本 | 时间 | 主控 | 连接 | 主要变化 |
|------|------|------|------|----------|
| V1 | 2025 Q3-Q4 | Adafruit HUZZAH32 | — | 硬件预研 |
| V2 | 2025.12-2026.01 | XIAO ESP32S3 | BLE HID | 首批组装，硅谷交付 |
| V3 | 2026.01-2026.02 | ESP32-S3 SuperMini | USB HID | BLE→USB 切换，12 套交付 |
| **V4** | **2026.04-2026.05** | **ESP32-S3 DevKitC N16R8** | **USB HID** | **模块化 PCB + NFC + 机械按钮，单套原型验证** |

## V4 进度

- [x] 原 12 套 BOM 定稿 + 报价（单套 ¥79，12 套总计约 ¥13,000，已取消）
- [x] 第一次需求会议（2026-04-01，与 Arthur/纪晟）
- [x] 2026-04-02 原 4+8 交付合同取消
- [x] 2026-04-29 与 Arthur 重启 V4：技术方案验证 + 单套原型，合同价 ¥5,000，目标 05-20 左右
- [ ] PCB 载板设计 (KiCad → JLCPCB)
- [x] USB-C 板载集成评估 — 方案 B (VBUS 被动注入) 选定，待面包板验证
- [ ] 按钮机构原型验证 (6.25U 卫星轴空格键，物料预计 04-02/03 到)
- [ ] NFC 数量确认 (1 vs 2，待杨炜乐讨论)
- [ ] NFC 驱动开发 (RC522 SPI)
- [ ] 旋钮 + NFC 协议规范文档
- [ ] 固件迁移 (SuperMini → DevKitC N16R8)
- [ ] 外壳适配 (Arthur Fusion 360，概念图已出)
- [ ] 单套原型制作 + 整机测试
- [ ] 交付技术验证结果和单套原型

## 构建与烧录

```bash
get_idf                                    # 激活 ESP-IDF 环境
idf.py set-target esp32s3                  # 设置目标芯片（首次）
idf.py build                               # 构建
idf.py -p /dev/cu.usbmodem* flash monitor  # 烧录并监控
```

## GPIO 引脚分配（V4 — DevKitC N16R8）

| 功能组 | GPIO | 备注 |
|--------|------|------|
| EC11 左旋钮 A/B/SW | 1, 2, 3 | 中断，10K 上拉 |
| EC11 右旋钮 A/B/SW | 4, 5, 6 | 中断，10K 上拉 |
| Action Button | 7 | Kailh 轴，按下接地 |
| WS2812B LED DIN | 8 | 串联 330R |
| RC522 NFC (FSPI) | 34-37 (CS/MOSI/SCK/MISO), 9 (RST) | 硬件 SPI |
| USB OTG | 19 (D-), 20 (D+) | 固定引脚 |

> V3 引脚分配（SuperMini）见 git 历史

## 文档索引

- [V4 待办事项](TODO.md)
- [V4 BOM 及报价分析](docs/project/CosmoRadio-V4-BOM及报价分析.md)
- [V4 PCB 载板规格](docs/project/CosmoRadio-V4-PCB-Spec.md)
- [项目主文档](docs/project/cosmoradio-yangweile-customized-hardware.md)
- [USB 接口方案](docs/hardware/usb.md)
- [NFC 模块方案](docs/hardware/nfc.md)
