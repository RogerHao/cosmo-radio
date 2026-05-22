# CosmoRadio

> 桌面"平行宇宙收音机"交互设备。客户定制硬件项目（技术验证 + 单套原型）。

## Project Overview

安卓平板 + 3D 打印外壳 + ESP32 USB HID 控制器 + 旋钮/按钮/NFC。

**当前版本：V4**（2026.04 — 2026.05，04/29 重启为技术方案验证 + 单套原型制作）

**Architecture**: Android Tablet <-- USB HID --> ESP32-S3 DevKitC N16R8 (on custom PCB carrier board) <-- GPIO/SPI --> EC11 × 2 + Kailh BOX button + WS2812B LED + RC522 NFC

**客户**: 杨炜乐 / 上海噗噗噗教育科技有限公司。R1-R3 已交付（¥26,000 已结清）。R4 原 4+8 批量交付合同在 04/02 取消，04/29 与 Arthur 重启为 ¥5,000 技术验证 + 单套原型。

**项目文档**: `docs/project/` — BOM、PCB Spec、项目主文档、R1-R3 归档

## Current V4 Scope

- **Restart date**: 2026-04-29, agreement with Arthur
- **Contract price**: ¥5,000
- **Deliverable**: technical solution validation + one working prototype
- **Target delivery**: around 2026-05-20
- **Primary risk**: PCB design and validation
- **Out of scope**: original 4 complete devices + 8 electronics kits batch delivery

## Firmware

项目根目录即 ESP-IDF 项目（USB HID，TinyUSB）。BLE HID (V1/V2) 已删除，见 git 历史。

## Build Commands

```bash
get_idf                                    # 激活 ESP-IDF 环境
idf.py set-target esp32s3                  # 设置目标芯片（首次）
idf.py build                               # 构建
idf.py -p /dev/cu.usbmodem* flash monitor  # 烧录 + 监控
idf.py fullclean                           # 清理构建
```

## Code Architecture

```
cosmo-radio/                       # 项目根 = ESP-IDF 项目
├── main/
│   ├── tusb_hid_example_main.c    # Entry point, USB HID + ASCII→HID encoder
│   ├── input_handler.c/h          # GPIO interrupt-based input (button + 2 EC11)
│   ├── nfc_handler.c/h            # RC522 SPI scan + NDEF Text Record parser
│   └── led_indicator.c/h          # Onboard WS2812 RGB control (GPIO48)
├── sdkconfig.defaults
└── CMakeLists.txt
```

## GPIO Pin Assignments (V4 PCB r1.0 — ESP32-S3 DevKitC N16R8)

> **本节是项目里所有 GPIO 信息的唯一来源**（PCB 物理事实）。下游文档只引用、不复述具体数字。
> **GPIO 终版（2026-05-22 PCB r1.0 实焊定稿）**——飞线 (2026-05-06) → Plan v2 KiCad (2026-05-18) → **JLCEDA P1 手绘打板 + 实焊验证 (2026-05-22)**。
> ⚠️ N16R8 = 8MB **octal** PSRAM — GPIO 26-37 全段被内部 octal SPI flash + PSRAM 占用，不可使用。

| GPIO | Schematic Net | Connector (PCB r1.0) | Notes |
|------|---------------|---------------------|-------|
| 1  | BTN          | J1 pin 1 (2P) | Kailh BOX action button, internal pull-up |
| 42 | EC11-L-A     | J3 pin 1 (5P) | 10kΩ pull-up + 10nF to GND (R1+C1) |
| 41 | EC11-L-B     | J3 pin 3      | 10kΩ pull-up + 10nF to GND (R2+C2) |
| 40 | EC11-L-SW    | J3 pin 5      | internal pull-up |
| 17 | EC11-R-A     | J4 pin 5 (5P) | ⚠ 固件角色重指派，见下方 walkaround |
| 18 | EC11-R-B     | J4 pin 3      | ⚠ 同上 |
| 8  | EC11-R-SW    | J4 pin 1      | ⚠ 同上 |
| 16 | NFC_SDA (CS) | J2 pin 1 (8P) | software-driven CS |
| 15 | NFC_SCK      | J2 pin 2      | SPI2 (FSPI) via GPIO Matrix |
| 7  | NFC_MOSI     | J2 pin 3      | SPI2 via GPIO Matrix |
| 6  | NFC_MISO     | J2 pin 4      | SPI2 via GPIO Matrix |
| 5  | NFC_IRQ      | J2 pin 5      | low-active, 接线预留，固件未用 |
| 4  | NFC_RST      | J2 pin 8      | low-active |
| 19 | USB_DM       | J5 (4P)       | OTG dongle D-, 同时是 DevKitC native USB-C 上的 D- |
| 20 | USB_DP       | J5 (4P)       | OTG dongle D+ |
| 48 | (board RGB)  | DevKitC 板载  | WS2812B onboard，不外接 |
| 2, 9, 10, 11, 12, 13, 14, 38, 39, 47 | — | — | Reserved / 可扩展 |
| 0 (BOOT), 3, 45, 46 | — | — | ⚠ Strapping — 输入需确保 boot 时电平不被强拉 |
| 43 (TX), 44 (RX)   | — | — | ⚠ UART0 — 烧录/monitor 占用 |
| 26-37              | — | — | 🔒 Octal PSRAM 内部占用 |

PCB r1.0 连接器物理位置（用户俯视）：
- **PCB 顶部**（近 USB-C 端）: J5
- **PCB 左侧**: J1（靠前）、J3（中段）
- **PCB 右侧**: J4（中段）、J2（底部）

> ⚠️ **运行时不要插 DevKitC 板载 OTG USB-C**：GPIO19/20 同时连到了排针（→ J5 → dongle 注入 VBUS）和 native USB-C 焊盘，外部插 USB-C 会跟 dongle 注入冲突。固件烧录走 DevKitC **UART USB-C**（不是 OTG USB-C）。PCB 顶丝印有 "OTG USB-C: DO NOT PLUG" 警示。

### PCB r1.0 EC11-R 接线 walkaround（2026-05-22）

P1.epro2 原理图把 J3 (EC11-L) 和 J4 (EC11-R) 的引脚顺序画反了——J3 = `[A, GND, B, GND, SW]`，J4 = `[SW, GND, B, GND, A]`。一根 5P 端子线只能压一种顺序，因此装上 J4 时**物理 EC11 的 A/B/SW 三根线相对网络名整体错位**。

固件在 `main/input_handler.c` 用 `#define` 把 `GPIO_ENC2_A` / `_B` / `_SW` 重新指派到正确的 GPIO 上（**具体哪个 GPIO 扮演哪个角色，以那个 .c 文件为准**），同时把 A/B 互换一次让两个旋钮"向内拨"时指针方向一致。**硬件不用改、固件烧好即可。**

➡️ 长期修复（PCB r1.1）：把 J4 改成跟 J3 对称的 `[A, GND, B, GND, SW]`，去掉固件的 walkaround。

### 关于"用户左/右"的语义

GPIO 表里 "EC11-L" 指**用户视角的左旋钮**（用户左手摸到的那个）。固件代码里 `ENC1` 别名即 EC11-L，`ENC2` 即 EC11-R。

### EC11 接线说明（V4）

EC11 是纯机械开关，**不需要 VCC**，3 个 IO + GND 即可工作（C / SW2 都接 GND，A / B / SW1 接 GPIO）。
- A/B 信号线外加 **10kΩ 上拉 + 10nF 对地**（RC 时间常数 100µs），既增强边沿陡度又抑制机械抖动
- SW 仅靠内部上拉
- 4 个 R (R1-R4) + 4 个 C (C1-C4) 全部 0805 贴片，紧贴 J3/J4 摆放

### V3 GPIO (SuperMini — deprecated, see git history)

## HID Key Mappings

| Input | HID Key | Description |
|-------|---------|-------------|
| Action Button press | Enter | Confirm action |
| Action Button release | (key up) | |
| Encoder 1 CW | Up arrow | Frequency up |
| Encoder 1 CCW | Down arrow | Frequency down |
| Encoder 1 SW press | F1 | (placeholder, 平板侧可重新映射) |
| Encoder 1 SW release | (key up) | |
| Encoder 2 CW | Right arrow | Mode next |
| Encoder 2 CCW | Left arrow | Mode previous |
| Encoder 2 SW press | F2 | (placeholder, 平板侧可重新映射) |
| Encoder 2 SW release | (key up) | |
| NFC tag scan (NDEF Text 写入) | `<payload>\n` | **主路径**：固件读 NDEF Text Record，原样键入 payload + Enter（无前缀） |
| NFC tag scan (无 NDEF / 兜底) | `NFC:<UID_HEX>\n` | 兜底：空白卡 / 非 NTAG / 解析失败时类型 UID，方便诊断 |

### NFC Tag 内容编码契约

- **写入介质**：NTAG21x 用户内存（page 4+），NDEF Text Record，UTF-8
- **payload 字符集**：`[A-Za-z0-9 # : / . - _]`（其他字符 HID 表无法转码会被静默跳过）
- **payload 长度上限**：32 字节（超长截断）
- **重复触发抑制**：同 UID 1.5s 内静默丢弃（应对 RC522 心跳抖动）
- **生产流程**：用任一手机 NFC writer app（NFC Tools 等）写卡，固件零工具依赖
- **平板侧解析**：以 `\n` 分行，`NFC:` 开头是 UID 兜底；其余整行可能是 NFC payload 原文，也可能是 HID 键事件（旋钮箭头/Enter/F1/F2 等），平板侧需自行区分

## Hardware Target (V4)

| Board | Flash | PSRAM |
|-------|-------|-------|
| ESP32-S3 DevKitC N16R8 | 16MB | 8MB |

- **Power**: 外部充电器 → 一分二 OTG + 充电线材内置子板 (YK16-09E V1) → 平板（被动 SS34 注入方案已实测失败，弃用）
- **PCB**: Custom carrier board, **100×100mm 双面** (JLCPCB 免费打样), XH2.54 connectors
- **DevKitC 安装**: **两条 1×22 单排母**（不是 2×22 双排，单排好焊好对齐），间距 22.86mm
- **Connectors** (PCB r1.0 编号): J1 (Action Button 2P, **PCB 左侧靠前**), J2 (NFC RC522 8P, **PCB 右侧底**), J3 (EC11-L 5P, **PCB 左侧中段**), J4 (EC11-R 5P, **PCB 右侧中段**), J5 (OTG dongle USB 4P, **PCB 顶部**)
- **No external LED**: 取消外接 WS2812B，使用 DevKitC 板载 GPIO48 RGB LED 做调试指示
- **被动元件**: 4× 10kΩ 0805 + 4× 10nF 0805（EC11 A/B 信号 RC 去抖），其余零无源件
- **Screws**: M2.5×10 平头内六角 (flat head hex socket)
- **Speaker**: 使用平板自带音响，不外接
- **Top button**: 键盘轴 + 6.25U 卫星轴空格键 (~10cm)
- **Encoder press**: EC11 SW 引脚已接线，功能可选不用
- **NFC count**: 1 个 RC522 mini 模块（2026-05-06 确认，最终方案）
- **Cost**: 单套 ¥79（不含线材），主控板 ~¥32 (40%)

## PCB Design

Carrier board designed in KiCad, manufactured by JLCPCB.

- Spec: `docs/project/CosmoRadio-V4-PCB-Spec.md`
- BOM: `docs/project/CosmoRadio-V4-BOM及报价分析.md`

Key design decisions:
- ESP32-S3 DevKitC plugs into **两条 1×22 单排母** (not soldered, easy to swap)
- All peripherals connect via XH2.54 JST connectors (zero-solder assembly for peripherals)
- NFC uses SPI2 via GPIO Matrix (GPIO 4/5/6/7/15/16, not hardware-fixed FSPICLK pins — those are inside the octal PSRAM range)
- EC11 A/B signal lines: external 10kΩ pull-up + 10nF RC debounce filter (τ=100µs) — eliminates the "not crisp one-detent-per-click" issue seen on breadboard with internal pull-ups only
- OTG dongle (YK16-09E V1) integrated as internal sub-board: USB-A 母座 拆掉后 4 飞线 → J5 4P → DevKitC 排针 GPIO19/20 + 5V/GND
- **Plan v2 详细方案**：`~/.claude/plans/iridescent-snuggling-planet.md`

## HID Input Test Tool

```
test/hid-test.html
```

Three-circle layout matching physical mask. Transfer to device and open in browser for HID input testing.

## Known Limitations / Technical Debt

### NFC SPI 时钟暂用 1MHz

飞线版本 RF 发射时电源/地噪声会让默认 5MHz SPI 在 SELECT 命令上偶发 RX timeout。目前在 `main/nfc_handler.c` 显式设置 `clock_speed_hz = 1000000` 规避。**PCB 版本可以尝试拉回 5MHz**，铜箔走线 + 适当去耦电容后应该能稳定。

### Resolved (history)

- ~~EC11-R 右旋钮方向异常 (PCB r1.0)~~：2026-05-22 固件 walkaround 修复。打板后右旋钮表现为"任意方向转都是 CW、按下变 CCW"——根因是 P1.epro2 把 J4 的引脚顺序画成跟 J3 镜像（J3=`A,GND,B,GND,SW`，J4=`SW,GND,B,GND,A`），单一线序在 J4 上把物理 A 接到 EC11-R-SW net、SW 接到 EC11-R-A net。固件在 `main/input_handler.c` 重新指派 `GPIO_ENC2_A/B/SW`（18/8/17）并 swap A↔B 让左右旋钮"向内拨"方向一致。PCB r1.1 应把 J4 改回对称排列再去掉 walkaround。
- ~~HID Key Report 无多键支持~~：2026-05-08 已用 `s_pressed_keys[6]` + `s_hid_mutex` 重构 (`main/tusb_hid_example_main.c`)。input + NFC 两条 task 现在通过同一把锁串行化报告提交，按住 F1 + 转旋钮等组合能正确发出。
- ~~sdkconfig 仍是 V3 4MB 配置~~：2026-05-08 升级为 N16R8 (16MB QIO flash + 8MB Octal PSRAM @ 80MHz)。bootloader 不再警告 size mismatch；PSRAM 启用后 heap 可在大 buffer 场景自动外溢到 SPIRAM。
- ~~NFC 模块未接 / 无响应导致整机 reboot loop~~：2026-05-10 修复。`nfc_handler_init/start` 不再用 `ESP_ERROR_CHECK` 包裹，失败时只 `ESP_LOGW` 并继续，旋钮/按钮/HID 在裸板（无 RC522）场景照常工作。**约束**：NFC 是可选外设，新增外设时考虑同样的"失败降级"策略；只对"系统级别不可恢复"的失败用 `ESP_ERROR_CHECK`（mutex 创建、TinyUSB 安装等）。

## Documentation Structure

```
docs/
├── assets/          # 元器件引脚图、原理图、产品截图 (PNG)
├── firmware/        # 固件实现文档
├── hardware/        # 硬件模块设计方案 (每个模块一个 .md)
├── legacy/          # 弃用内容 (E5Ultra, 旧参考)
└── project/         # BOM、PCB Spec、项目主文档、R1-R3 归档
    ├── cosmoradio-yangweile-customized-hardware.md  # 项目主文档（合同、交付、财务）
    ├── CosmoRadio-V4-BOM及报价分析.md
    ├── CosmoRadio-V4-PCB-Spec.md
    └── archive-r1-r3/                               # V1-V3 历史交付文档
```

## Related Repos (archived, not active)

| Repo | What | Status |
|------|------|--------|
| [cosmo-pager](https://github.com/RogerHao/cosmo-pager) | ESP32-S3 嵌入式方案（3.5寸触屏），被 USB HID 方案取代 | Archived |
| [astro-pager](https://github.com/RogerHao/astro-pager) | 超脑AI孵化器学生黑客松硬件项目（2024 Q4），Alien Agent 生态 | Archived |

### 新增元器件/模块规范

当引入新硬件模块时，按以下步骤更新文档：

1. **硬件设计文档** `docs/hardware/<module-name>.md`
   - 模块功能、选型理由、接口定义、电路方案
   - 引用 `docs/assets/` 中的引脚图

2. **引脚图/参考图** `docs/assets/<module>-<description>.png`
   - 命名：`<模块名>-<内容>.png`（如 `rc522-mini-pinout-and-schematic.png`）
   - 产品页截图、引脚定义图、原理图

3. **BOM 更新** `docs/project/CosmoRadio-V4-BOM及报价分析.md`
   - 新增采购条目（编号、规格、采购量、单价、链接索引）
   - 更新采购总计和单套成本

4. **GPIO/引脚分配** 更新本文件 (CLAUDE.md) 的 GPIO Pin Assignments 表

5. **PCB 更新**（如需要）更新 `docs/project/CosmoRadio-V4-PCB-Spec.md` 连接器定义

### 文件命名

- 目录名：小写英文，连字符分隔 (`hardware/`, `legacy/`)
- 文档：小写英文，连字符分隔 (`nfc.md`, `usb.md`)
- 图片：`<模块>-<描述>.png` (`usb-c-breakout-6p-pinout.png`)
- 项目文档 (docs/project/)：中文标题允许

## Documentation Language

- Code comments: English
- Project docs (README, docs/): Chinese
