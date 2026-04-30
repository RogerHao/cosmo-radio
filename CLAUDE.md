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
│   ├── tusb_hid_example_main.c    # Entry point, USB HID + input handling
│   ├── input_handler.c/h          # GPIO interrupt-based input
│   └── led_indicator.c/h          # WS2812 LED control
├── sdkconfig.defaults
└── CMakeLists.txt
```

## GPIO Pin Assignments (V4 — ESP32-S3 DevKitC N16R8)

| Function | GPIO | Notes |
|----------|------|-------|
| EC11 Left A | 1 | Interrupt, 10K pull-up |
| EC11 Left B | 2 | Interrupt, 10K pull-up |
| EC11 Left SW | 3 | Press = GND |
| EC11 Right A | 4 | Interrupt, 10K pull-up |
| EC11 Right B | 5 | Interrupt, 10K pull-up |
| EC11 Right SW | 6 | Press = GND |
| Action Button | 7 | Kailh BOX switch, press = GND |
| WS2812B DIN | 8 | Series 330R resistor |
| RC522 NFC CS | 34 (FSPICS0) | Hardware SPI |
| RC522 NFC MOSI | 35 (FSPID) | Hardware SPI |
| RC522 NFC SCK | 36 (FSPICLK) | Hardware SPI |
| RC522 NFC MISO | 37 (FSPIQ) | Hardware SPI |
| RC522 NFC RST | 9 | Reset |
| RC522 NFC IRQ | 10 | Interrupt, optional |
| USB D- | 19 | Native USB OTG (fixed) |
| USB D+ | 20 | Native USB OTG (fixed) |
| Reserved | 11-18 | Expansion |

All input GPIOs use pull-up resistors (external 10K on PCB for EC11, internal for button).
NFC uses FSPI hardware SPI bus (GPIO34-37) for best performance.

### V3 GPIO (SuperMini — deprecated, see git history)

## HID Key Mappings

| Input | HID Key | Description |
|-------|---------|-------------|
| Button press | Enter | Confirm action |
| Button release | (key up) | |
| Encoder 1 CW | Up arrow | Frequency up |
| Encoder 1 CCW | Down arrow | Frequency down |
| Encoder 2 CW | Right arrow | Mode next |
| Encoder 2 CCW | Left arrow | Mode previous |
| NFC tag scan | `NFC:<UID_HEX>\n` | HID 键盘逐字符键入，平板端解析 UID 映射动作 |

## Hardware Target (V4)

| Board | Flash | PSRAM |
|-------|-------|-------|
| ESP32-S3 DevKitC N16R8 | 16MB | 8MB |

- **Power**: 外部充电器 → J6 VBUS → PCB → J5 VBUS → 平板（方案 B 被动注入）
- **PCB**: Custom carrier board, 80×50mm, XH2.54 connectors
- **Connectors**: J1/J2 (EC11 5P), J3 (Top module 3P/4P), J4 (NFC 8P), J5 (USB-C 平板输出), J6 (USB-C 充电输入)
- **Screws**: M2.5×10 平头内六角 (flat head hex socket)
- **Speaker**: 使用平板自带音响，不外接
- **Top button**: 键盘轴 + 6.25U 卫星轴空格键 (~10cm)
- **Encoder press**: EC11 SW 引脚已接线，功能可选不用
- **NFC count**: ⚠️ 待确认 1 vs 2 个 RC522 模块（影响 GPIO/PCB）
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

### HID Key Report: No Multi-key Support

Current `send_key_up()` releases ALL keys (sends empty report). Fine for single-input use case, will need fix for hold-button-while-rotating scenarios.

**Fix when needed**: Maintain `pressed_keys[6]` state array, add/remove individual keycodes per event.

### V4 Migration Pending

Firmware still targets SuperMini GPIO mapping. V4 migration tasks:
- Remap all GPIOs to DevKitC N16R8 pin assignments
- Add RC522 NFC SPI driver (FSPI bus)
- Add Kailh button handler (GPIO7)
- Update `sdkconfig.defaults` for N16R8 flash/PSRAM config

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
