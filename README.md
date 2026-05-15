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

输入设备通过 ESP32 USB HID 协议作为标准键盘连接到安卓平板，事件以模拟按键传递：

| 输入 | 映射 | 功能 |
|------|------|------|
| 顶部 Action 按钮 | Enter 键 | 确认选择 |
| EC11 左 旋转 | 上/下方向键 | 调节频率 |
| EC11 左 按下 | F1（占位，平板可重映射） | 扩展功能位 |
| EC11 右 旋转 | 左/右方向键 | 切换模式 |
| EC11 右 按下 | F2（占位，平板可重映射） | 扩展功能位 |
| NFC 卡（写了 NDEF Text） | `<payload>\n` | **主路径**：固件读卡上写入的字符串，原样键入 + Enter（无前缀） |
| NFC 卡（空白 / 非 NDEF） | `NFC:<UID>\n` | 兜底：识别失败时键入 UID 方便诊断 |

### NFC 协议设计

为了让产品身份与 NFC 卡 UID 解耦（UID 不可控且需要逐张录入），固件读取卡上的 NDEF Text Record 内容作为产品 ID。流程：

1. 出厂前用任一手机 NFC 工具 app 把每张卡写入特定字符串（如 `R01`、`track-42`）
2. 用户放卡 → 固件读 NDEF → HID 键入 `R01\n`
3. 平板侧 Android 应用结合页面状态/输入长度判断当前是 NFC payload 还是普通 HID 键入；`NFC:` 前缀仅出现在 UID 兜底路径，方便区分

### LED 状态指示（DevKitC 板载 GPIO48 RGB LED）

| 事件 | LED 颜色 |
|------|----------|
| 按钮按下 | 红色 |
| 按钮释放 | 熄灭 |
| NFC 识别中（键入期间） | 蓝色 |
| 启动完成 | 绿色闪烁 |

## 项目结构

```
cosmo-radio/                            # 项目根 = ESP-IDF USB HID 固件
├── main/
│   ├── tusb_hid_example_main.c         # 入口：USB HID + ASCII→HID 编码
│   ├── input_handler.c/h               # GPIO 中断输入（按钮 + 双 EC11）
│   ├── nfc_handler.c/h                 # RC522 SPI 扫描 + NDEF Text 解析
│   └── led_indicator.c/h               # 板载 WS2812 RGB（GPIO48）
├── sdkconfig.defaults
├── CMakeLists.txt
├── pcb/                                # KiCad 工程 + JLCPCB 上板资料（待开始）
├── docs/
│   ├── assets/                         # 元器件引脚图、原型照片
│   ├── firmware/                       # 固件实现文档
│   ├── hardware/                       # 硬件模块设计方案（NFC、USB...）
│   ├── legacy/                         # 弃用内容
│   └── project/ → codex                # BOM、PCB Spec、项目主文档
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

**2026-05-07 整体技术方案验证完成 ✅**——电气、固件、NFC 协议端到端跑通。后续是 PCB 板设计 + 结构件设计。

电气与固件：
- [x] 原 12 套 BOM 定稿 + 报价（单套 ¥79，已取消）
- [x] 2026-04-29 与 Arthur 重启 V4：技术方案验证 + 单套原型，合同价 ¥5,000
- [x] USB-C 充电方案：被动 SS34 注入实测失败 → 改为内置一分二 OTG 线材小板（已采购验证）
- [x] NFC 数量确认：1 个 RC522 mini（2026-05-06）
- [x] 万能板飞线原型焊接 + GPIO 验证（2026-05-06，[图片](docs/assets/v4-handmade-prototype-2026-05-06.jpg)）
- [x] 固件 V4 GPIO 迁移（SuperMini → DevKitC N16R8）
- [x] NFC 驱动开发（RC522 SPI + NDEF Text Record 解析）
- [x] 旋钮 + NFC HID 协议定稿（`#<payload>\n` + UID 兜底）
- [x] 端到端验证：iPhone 写卡 → ESP32 读 NDEF → HID 键入 `#112358\n`（2026-05-07）

待办（V4 收尾）：
- [ ] PCB 载板设计（KiCad → JLCPCB），把万能板飞线版固化为正式板
- [ ] Arthur 寄新 Tab A9（原 Pad 已损坏）→ HID + 充电链路回归
- [ ] 按钮机构验证（6.25U 卫星轴空格键 + 3D 打印键帽）
- [ ] 外壳 3D 模型适配（Arthur Fusion 360）+ 打印装配
- [ ] 单套整机原型组装 + 整机测试
- [ ] 交付技术验证报告 + 单套原型

## 构建与烧录

```bash
get_idf                                    # 激活 ESP-IDF 环境
idf.py set-target esp32s3                  # 设置目标芯片（首次）
idf.py build                               # 构建
idf.py -p /dev/cu.usbmodem* flash monitor  # 烧录并监控
```

## GPIO 引脚分配（V4 终版，2026-05-06 定稿）

> N16R8 = 8MB octal PSRAM，**GPIO 26-37 全段被内部 octal SPI flash + PSRAM 占用，不可使用**。详情和连接器/排针对应表见 [CLAUDE.md](CLAUDE.md)。

| 功能 | GPIO | 连接器 |
|------|------|--------|
| Action Button | 1 | J3 (右上 2P) |
| EC11 左 A/B/SW | 17, 18, 8 | J1 (左中 5P) |
| EC11 右 A/B/SW | 42, 41, 40 | J2 (右中 5P) |
| RC522 NFC RST/IRQ/MISO/MOSI/SCK/CS | 4 / 5 / 6 / 7 / 15 / 16 | J4 (左上 8P, SPI2 GPIO Matrix) |
| USB OTG D-/D+ | 19 / 20 | OTG 子板（固定） |
| 板载 RGB LED | 48 | DevKitC 板载，无外接 LED |

> V3 引脚分配（SuperMini）见 git 历史

## 文档索引

- [V4 待办事项](TODO.md)
- [V4 BOM 及报价分析](docs/project/CosmoRadio-V4-BOM及报价分析.md)
- [V4 PCB 载板规格](docs/project/CosmoRadio-V4-PCB-Spec.md)
- [项目主文档](docs/project/cosmoradio-yangweile-customized-hardware.md)
- [USB 接口方案](docs/hardware/usb.md)
- [NFC 模块方案](docs/hardware/nfc.md)
- [OTG 一分二适配器（内置子板）](docs/hardware/otg-adapter.md)
