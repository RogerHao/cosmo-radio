# CosmoRadio

可穿梭于平行宇宙的 Radio 接收器

## 项目简介

桌面交互设备，安卓平板 + 3D 打印外壳 + ESP32 USB HID 控制器。造型参考 iMac G3 斜面站立式设计，用户通过旋钮调频、NFC 刷卡切换内容，仿佛在平行宇宙间搜索信号。

**当前版本：V4**（2026.04 — 2026.05）— 技术方案验证 + 单套原型制作

> 2026-04-29 与噗噗噗公司确认重启协议：第四期缩小为技术方案验证和单套原型制作，合同价 ¥5,000，目标 2026-05-20 左右交付。原 4 套整机 + 8 套电子套件交付范围已取消。

## 产品形态

- **外观风格**: iMac G3 斜面站立式，模块化三件式（主体 + 顶部 + 背板）
- **显示设计**: 三圆形遮罩（矩形屏幕通过开孔分割为三个圆形显示区）
- **输入方式**: 2 × EC11 旋钮 + Kailh 机械轴按钮 (6.25U 卫星轴) + NFC 刷卡
- **状态指示**: WS2812 RGB LED

## 硬件组成（V4）

| 组件 | 规格 | 用途 |
|------|------|------|
| 安卓平板 | 客户自购 (Samsung Galaxy Tab A9 8.7") | 主机，运行 Web 应用 |
| ESP32-S3 DevKitC N16R8 | 36+ GPIO / USB OTG / 16MB Flash / 8MB PSRAM | HID 控制器 |
| 定制 PCB 载板 | **100×100mm 双面** (JLCPCB 免费打样)，**两条 1×22 单排母** + 5 路 XH2.54 + 4× 10kΩ/10nF 去抖 | 消灭飞线，XH2.54 即插即用 |
| EC11 旋钮 × 2 | 15mm 半轴柄旋转编码器 | 频率调节 / 模式切换 |
| Kailh BOX 轴 + 6.25U 卫星轴 | 机械键盘蝴蝶结构，3.6mm 行程 | Action Button |
| RC522 mini NFC 模块 × 1 | 13.56MHz SPI，读 NTAG215 NDEF | NFC 刷卡交互（最终 1 个，2026-05-06 确认） |
| **OTG dongle (YK16-09E V1) 内置子板** | 拆 USB-A 母座，4 飞线接 J5 → DevKitC 排针 GPIO19/20 + 5V/GND | 平板 Data Host + Power Sink 同时工作 |
| 板载 RGB LED | DevKitC GPIO48 WS2812B | 状态指示（不外接 LED）|

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
│   ├── input_handler.c/h               # GPIO 中断 + 物理电平复核输入（按钮 + 双 EC11）
│   ├── nfc_handler.c/h                 # RC522 SPI 扫描 + NDEF Text 解析
│   └── led_indicator.c/h               # 板载 WS2812 RGB（GPIO48）
├── sdkconfig.defaults
├── CMakeLists.txt
├── pcb/                                # 嘉立创EDA r1.0 验证版工程 + 制造文件
├── models/                             # 3D 模型（v4 = 最终交付集）
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

**2026-06-01 第四期结项 ✅**——技术验证 2026-05-22 完成，2026-05-31 客户收到成品交付，2026-06-01 开票收款 ¥5,000（累计 ¥31,000 全部结清）。

**2026-05-22 V4 技术验证交付完成 ✅**——PCB r1.0 实焊 + 双板并排功能验证通过，单套原型装配完成。详见 [V4 资料包](project/v4/shared/先读说明.md)。

已交付：
- [x] 2026-04-29 与噗噗噗公司重启 V4：技术方案验证 + 单套原型，合同价 ¥5,000
- [x] USB-C 充电方案：被动 SS34 注入实测失败 → 内置一分二 OTG 线材子板（YK16-09E V1）验证通过
- [x] NFC 数量确认：1 个 RC522 mini（2026-05-06）
- [x] 万能板飞线原型焊接 + GPIO 验证（2026-05-06，[图片](docs/assets/v4-handmade-prototype-2026-05-06.jpg)）
- [x] 固件 V4 GPIO 迁移（SuperMini → DevKitC N16R8）+ HID 多键并发
- [x] NFC 驱动开发（RC522 SPI + NDEF Text Record 解析）
- [x] 旋钮 + NFC HID 协议定稿（NDEF payload 原样键入，无前缀 + `NFC:<UID>` 兜底）
- [x] 端到端验证：iPhone 写卡 → ESP32 读 NDEF → HID 键入（2026-05-15 实卡验证）
- [x] **PCB r1.0 载板**：嘉立创EDA手绘 → 打样 → 实焊 → 功能验证（源文件 `pcb/cosmoradio-v4-jlceda/`）
- [x] 单套整机原型装配（[Fusion 装配 `cosmo-radio-v4-prototype`，模型 `models/v4/`]）
- [x] Action Button 最终手感、整机热稳定 + 2h 充电链路、NFC 穿透壁厚读距回归（2026-05-24 结项确认）
- [x] 固件安全回归：按钮释放物理电平复核、HID key-up 重试、NFC 启动后台重试（2026-05-28 PCB 实机烧录验证）

结项归档待办：
- [ ] PCB 制造文件从嘉立创EDA导出归档（清单见 [pcb/README.md](pcb/README.md)）
- [ ] r1.1 优化（J4 改对称去 walkaround、NFC 5MHz）— 见验证报告 §6

## 构建与烧录

```bash
get_idf                                    # 激活 ESP-IDF 环境
idf.py set-target esp32s3                  # 设置目标芯片（首次）
idf.py build                               # 构建
idf.py -p /dev/cu.usbmodem* flash monitor  # 烧录并监控
```

> ⚠️ 烧录必须走 DevKitC 的 **UART USB-C** 口（本板 UART 桥是 WCH CH343），别插 native/OTG 口——两个口在 macOS 下都枚举成 `usbmodem`，靠 USB VID 区分（`0x1A86`=UART 桥可烧 / `0x303A`=OTG 口烧不了）。详见 [CLAUDE.md "烧录端口"](CLAUDE.md)。

## GPIO 引脚分配

完整 GPIO → net → 连接器 映射、PCB 物理位置、PCB r1.0 EC11-R 接线 walkaround 全部见 [CLAUDE.md "GPIO Pin Assignments"](CLAUDE.md#gpio-pin-assignments-v4-pcb-r10--esp32-s3-devkitc-n16r8)。固件里 `main/input_handler.c` 和 `main/nfc_handler.c` 的 `#define` 是各自的 GPIO 角色权威源。

> 简易记忆：左旋钮 → J3，右旋钮 → J4 (引脚顺序跟 J3 镜像)，BTN → J1，NFC → J2，USB → J5。V3 引脚分配（SuperMini）见 git 历史。

## 文档索引

**交付（客户）**
- [V4 资料包先读](project/v4/shared/先读说明.md) ⭐ 交付核心
- [产品功能与当前边界](project/v4/shared/产品功能与当前边界.md)
- [平板对接协议](project/v4/shared/平板对接协议.md)
- [硬件参考资料](project/v4/shared/硬件参考资料.md)
- [复现与维护指南](project/v4/shared/复现与维护指南.md)
- [物料清单与采购参考](project/v4/shared/物料清单与采购参考.md)

**硬件方案**
- [USB 接口方案](docs/hardware/usb.md)
- [NFC 模块方案](docs/hardware/nfc.md)
- [OTG 一分二适配器（内置子板）](docs/hardware/otg-adapter.md)

**内部**
- [V4 内部成本备忘录](project/v4/internal/CosmoRadio-V4-内部成本备忘录.md) — 内部，不交付
- [V4 项目内部主文档](project/v4/internal/cosmoradio-yangweile-customized-hardware.md) — 内部，不交付
- [V4 待办事项](TODO.md)
