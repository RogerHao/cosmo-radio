# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CosmoRadio — 桌面"平行宇宙收音机"交互设备。安卓平板 + 3D 打印外壳 + ESP32 USB HID 控制器 + 旋钮/按钮/NFC。

**当前版本：V4**（2026.04 — 2026.05）

**Architecture**: Android Tablet <-- USB HID --> ESP32-S3 DevKitC N16R8 (on custom PCB carrier board) <-- GPIO/SPI --> EC11 × 2 + Kailh BOX button + WS2812B LED + RC522 NFC

**项目文档**: `docs/project/` (symlink → codex) — BOM、PCB Spec、项目主文档

## Firmware

| Firmware | Directory | Status |
|----------|-----------|--------|
| USB HID | `tusb_hid/` | **Active** — V3/V4 使用 |
| BLE HID | `firmware/` | Deprecated — V1/V2 遗留 |

## Build Commands

All firmware commands require ESP-IDF environment active:

```bash
# Initialize ESP-IDF environment (if using get_idf alias)
get_idf

# For BLE HID firmware
cd firmware

# For USB HID firmware
cd tusb_hid

# Set target chip (first time only)
idf.py set-target esp32s3

# Build firmware
idf.py build

# Flash to device
idf.py -p /dev/cu.usbmodem* flash

# Monitor serial output
idf.py -p /dev/cu.usbmodem* monitor

# Build + Flash + Monitor combined
idf.py -p /dev/cu.usbmodem* flash monitor

# Clean build
idf.py fullclean
```

## Code Architecture

```
tusb_hid/                          # USB HID (active)
├── main/
│   ├── tusb_hid_example_main.c    # Entry point, USB HID + input handling
│   ├── input_handler.c/h          # GPIO interrupt-based input
│   └── led_indicator.c/h          # WS2812 LED control
├── sdkconfig.defaults
└── CMakeLists.txt

firmware/                          # BLE HID (deprecated, V1/V2 only)
└── ...
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
| USB D- | 19 | Native USB OTG (fixed) |
| USB D+ | 20 | Native USB OTG (fixed) |
| Reserved | 10-18 | Expansion |

All input GPIOs use pull-up resistors (external 10K on PCB for EC11, internal for button).
NFC uses FSPI hardware SPI bus (GPIO34-37) for best performance.

### V3 GPIO (SuperMini — deprecated)

| GPIO | Function |
|------|----------|
| 1, 2 | Button A/B (redundant) |
| 3, 4 | Encoder 1 CLK/DT |
| 5, 6 | Encoder 2 CLK/DT |
| 7 | WS2812 LED |

## HID Key Mappings

| Input | HID Key | Description |
|-------|---------|-------------|
| Button press | Enter | Confirm action |
| Button release | (key up) | |
| Encoder 1 CW | Up arrow | Frequency up |
| Encoder 1 CCW | Down arrow | Frequency down |
| Encoder 2 CW | Right arrow | Mode next |
| Encoder 2 CCW | Left arrow | Mode previous |
| NFC tag scan | TBD | V4: mapped by UID → keycode |

## Hardware Target (V4)

| Board | Flash | PSRAM |
|-------|-------|-------|
| ESP32-S3 DevKitC N16R8 | 16MB | 8MB |

- **Power**: USB from tablet (5V via Type-C)
- **PCB**: Custom carrier board, 80×50mm, XH2.54 connectors
- **Connectors**: J1/J2 (EC11 5P), J3 (Top module 3P/4P), J4 (NFC 8P), J5 (USB-C)

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

## Documentation Language

- Code comments: English
- Project docs (README, docs/): Chinese
