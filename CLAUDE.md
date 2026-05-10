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

## GPIO Pin Assignments (V4 — ESP32-S3 DevKitC N16R8, finalized 2026-05-06)

> 万能板飞线优先，按"DevKitC 排针物理位置 ↔ 连接器物理位置"最近邻原则分配。
> ⚠️ N16R8 = 8MB **octal** PSRAM — GPIO 26-37 全段被内部 octal SPI flash + PSRAM 占用，不可使用。

| Function | GPIO | Connector | Notes |
|----------|------|-----------|-------|
| Action Button | 1 | J3 (右上 2P) | Kailh BOX, press = GND, internal pull-up |
| EC11 Left A | 17 | J1 (左中 5P) | internal pull-up |
| EC11 Left B | 18 | J1 (左中 5P) | internal pull-up |
| EC11 Left SW | 8 | J1 (左中 5P) | internal pull-up |
| EC11 Right A | 42 | J2 (右中 5P) | internal pull-up |
| EC11 Right B | 41 | J2 (右中 5P) | internal pull-up |
| EC11 Right SW | 40 | J2 (右中 5P) | internal pull-up |
| RC522 NFC RST | 4 | J4 (左上 8P) | low-active |
| RC522 NFC IRQ | 5 | J4 (左上 8P) | low-active, 当前固件未使用，仅接线预留 |
| RC522 NFC MISO | 6 | J4 (左上 8P) | SPI2 (FSPI) via GPIO Matrix |
| RC522 NFC MOSI | 7 | J4 (左上 8P) | SPI2 (FSPI) via GPIO Matrix |
| RC522 NFC SCK | 15 | J4 (左上 8P) | SPI2 (FSPI) via GPIO Matrix |
| RC522 NFC CS (SDA) | 16 | J4 (左上 8P) | software-driven |
| USB D- | 19 | OTG 子板 | Native USB OTG (fixed) |
| USB D+ | 20 | OTG 子板 | Native USB OTG (fixed) |
| Onboard RGB LED | 48 | DevKitC 板载 | WS2812B 板载，bring-up 调试用，不外接 LED |
| Reserved (free) | 2, 9, 10, 11, 12, 13, 14, 38, 39, 47 | — | 扩展可用 |
| ⚠️ Strapping | 0 (BOOT), 3, 45, 46 | — | 做输入需保证 boot 时电平不被强拉 |
| ⚠️ UART0 | 43 (TX), 44 (RX) | — | 烧录/monitor 占用，不要做关键输入 |
| 🔒 Octal PSRAM | 26-37 | — | 内部使用，不可分配 |

### EC11 接线说明（V4）

EC11 是纯机械开关，**不需要 VCC**，3 个 IO + GND 即可工作（C / SW2 都接 GND，A / B / SW1 接 GPIO，靠 ESP32 内部上拉拉高）。
飞线阶段使用内部 ~45kΩ 上拉；PCB 版本会外加 10kΩ 上拉提高抗噪与边沿锐度。

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
| NFC tag scan (NDEF Text 写入) | `#<payload>\n` | **主路径**：固件读 NDEF Text Record，把 payload 加 `#` 前缀 + Enter 键入 |
| NFC tag scan (无 NDEF / 兜底) | `NFC:<UID_HEX>\n` | 兜底：空白卡 / 非 NTAG / 解析失败时类型 UID，方便诊断 |

### NFC Tag 内容编码契约

- **写入介质**：NTAG21x 用户内存（page 4+），NDEF Text Record，UTF-8
- **payload 字符集**：`[A-Za-z0-9 # : / . - _]`（其他字符 HID 表无法转码会被静默跳过）
- **payload 长度上限**：32 字节（超长截断）
- **重复触发抑制**：同 UID 1.5s 内静默丢弃（应对 RC522 心跳抖动）
- **生产流程**：用任一手机 NFC writer app（NFC Tools 等）写卡，固件零工具依赖
- **平板侧解析**：以 `\n` 分行，`#` 开头是 NFC payload，`NFC:` 开头是 UID 兜底，其它键是输入设备事件

## Hardware Target (V4)

| Board | Flash | PSRAM |
|-------|-------|-------|
| ESP32-S3 DevKitC N16R8 | 16MB | 8MB |

- **Power**: 外部充电器 → 一分二 OTG + 充电线材内置子板 → 平板（被动 SS34 注入方案已实测失败，弃用）
- **PCB**: Custom carrier board, 80×50mm, XH2.54 connectors
- **Connectors**: J1 (EC11 左 5P, 左中), J2 (EC11 右 5P, 右中), J3 (Action Button 2P, 右上), J4 (NFC RC522 8P, 左上). USB / 充电由 OTG 一分二子板承担，无独立 J5/J6
- **No external LED**: 取消外接 WS2812B，使用 DevKitC 板载 GPIO48 RGB LED 做调试指示
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
- ESP32-S3 DevKitC plugs into 2×22P pin header sockets (not soldered)
- All peripherals connect via XH2.54 JST connectors (zero-solder assembly)
- NFC uses FSPI hardware SPI (GPIO34-37) for signal integrity
- External 10K pull-ups for EC11 (more reliable than internal pull-ups)

## HID Input Test Tool

```
test/hid-test.html
```

Three-circle layout matching physical mask. Transfer to device and open in browser for HID input testing.

## Known Limitations / Technical Debt

### NFC SPI 时钟暂用 1MHz

飞线版本 RF 发射时电源/地噪声会让默认 5MHz SPI 在 SELECT 命令上偶发 RX timeout。目前在 `main/nfc_handler.c` 显式设置 `clock_speed_hz = 1000000` 规避。**PCB 版本可以尝试拉回 5MHz**，铜箔走线 + 适当去耦电容后应该能稳定。

### Resolved (history)

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
