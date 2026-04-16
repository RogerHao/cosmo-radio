# CosmoRadio V4 — Motherboard PCB Spec

> 目标：消灭飞线，所有外设通过 JST 连接器即插即用，非技术人员可自行组装
> 底板空间：~220×150mm，顶部按钮模块独立，通过线缆连接

---

## 设计理念

**Carrier Board（载板）** — ESP32-S3 DevKitC N16R8 插入排母座，所有外设 JST 即插。

### 为什么选 ESP32-S3

| 对比项 | ESP32-S3 SuperMini (V3) | ESP32-S3 DevKitC N16R8 (V4) |
|--------|------------------------|----------------------|
| USB HID | TinyUSB ✓ | TinyUSB ✓（同一套代码） |
| BLE | 有（未用） | 无（不需要） |
| GPIO 数量 | 11 可用 | 36+ 可用 |
| 价格 (1688) | ¥15 | ¥8-10 |
| 尺寸 | 25×18mm | 70×25mm |
| 开发框架 | ESP-IDF / Arduino | ESP-IDF / Arduino（相同） |
| 固件迁移量 | — | 改 GPIO 编号，其余不变 |

ESP32-S3 有原生 USB OTG，TinyUSB HID 代码直接跑。去掉了不需要的 BLE radio，便宜 40%，GPIO 多 3 倍。220×150mm 底板装 70×25mm 的 DevKitC 绰绰有余。

### 结构变化：V3 → V4

```
V3: 底部万能板 ─── 两根高柱 ─── 顶部按钮
    (飞线缠绕，柱子传力到万能板，整体脆弱)

V4: 底板 PCB (220×150mm 空间) ──┬── JST 线缆 → 左右旋钮
                                ├── JST 线缆 → 顶部模块（按钮+LED）
                                ├── JST 线缆 → NFC 模块
                                └── USB-C → 平板
    (顶部独立模块，柱子取消，线缆走外壳内壁)
```

### V3 问题 → V4 解决

| V3 问题 | V4 方案 |
|---------|---------|
| 飞线杂乱，杜邦线无锁扣 | PCB 走线 + JST-XH 带卡扣连接器 |
| 两根高柱支撑，结构脆弱 | 顶部独立模块，取消柱子 |
| 万能板空间局促 | 220×150mm 底板空间，PCB 可以做大做舒服 |
| 组装需焊接技能 | 全插拔 + 螺丝固定，附图文指南 |
| SuperMini GPIO 不够 | S2 DevKitC 36+ GPIO，充裕 |

---

## 原理图

### ESP32-S3 DevKitC N16R8 引脚分配

ESP32-S3 DevKitC N16R8 有 2×22 排针 (44 pin)，GPIO 充裕。以下为逻辑分配：

```
功能组          GPIO          备注
─────────────────────────────────────────
EC11 左旋钮
  A             GPIO1         中断，10K 上拉
  B             GPIO2         中断，10K 上拉
  SW            GPIO3         按下接地

EC11 右旋钮
  A             GPIO4         中断，10K 上拉
  B             GPIO5         中断，10K 上拉
  SW            GPIO6         按下接地

Action Button
  SIG           GPIO7         Kailh 轴，按下接地

WS2812B LED
  DIN           GPIO8         串联 330Ω

RC522 NFC (SPI)
  SCK           GPIO36 (FSPICLK)   硬件 SPI
  MOSI          GPIO35 (FSPID)     硬件 SPI
  MISO          GPIO37 (FSPIQ)     硬件 SPI
  SDA/CS        GPIO34 (FSPICS0)   片选
  RST           GPIO9              复位

USB HID
  D+            GPIO20 (native)    USB OTG 固定引脚
  D-            GPIO19 (native)    USB OTG 固定引脚

Reserved (扩展)
  GPIO10-18     未分配              未来扩展用
─────────────────────────────────────────
```

> NFC 使用 FSPI 硬件 SPI 总线（GPIO34-37），这是 S2 的专用 SPI 引脚，性能最佳。
> USB OTG 固定在 GPIO19/20，不可更改。

### 连接器定义

| 连接器 | 型号 | 引脚数 | 引脚定义 | 线缆长度 | 方向 |
|--------|------|--------|----------|---------|------|
| J1 | JST-XH 5P | 5 | VCC, GND, A, B, SW | 20cm | → 左旋钮 |
| J2 | JST-XH 5P | 5 | VCC, GND, A, B, SW | 20cm | → 右旋钮 |
| J3 | JST-XH 4P | 4 | VCC, GND, SIG, LED_DIN | 25cm | → 顶部模块（按钮+LED） |
| J4 | JST-XH 7P | 7 | VCC, GND, SCK, MOSI, MISO, CS, RST | 15cm | → NFC 模块 |
| J5 | USB-C 母座 | — | USB 2.0 D+/D- | 板载 | → 外部线缆到平板 |
| U1 | 2×22 排母座 | 40 | ESP32-S3 DevKitC N16R8 插入 | 板载 | — |

> J3 合并了按钮和 LED 到同一个连接器 — 它们都在顶部模块，走同一根线缆。
> 顶部模块内部有一小块子板/手焊板，接收 J3 的 4 根线分配到 Kailh 轴和 WS2812B。

### 电路要点

1. **USB HID 通路**：DevKitC USB 端口 → PCB 走线 → J5 USB-C 母座 → 线缆 → 平板 USB 口
2. **供电**：平板 USB 5V → DevKitC → PCB 5V/3V3 轨道 → 各模块
3. **上拉电阻**：EC11 A/B 各 10KΩ 上拉到 3V3（共 4 个，含 SW 可选 6 个）
4. **NFC SPI**：FSPI 硬件总线，走线尽量短，信号线间距 ≥ 0.3mm
5. **LED 数据线**：WS2812B DIN 串联 330Ω 电阻
6. **去耦电容**：NFC 模块 VCC 旁放 100nF 陶瓷电容

---

## PCB 物理规格

| 参数 | 值 |
|------|-----|
| 尺寸 | 80mm × 50mm（底板空间 220×150，PCB 不需要填满） |
| 层数 | 2 层 |
| 板厚 | 1.6mm |
| 铜厚 | 1oz |
| 表面处理 | HASL（无铅） |
| 阻焊 | 黑色 |
| 丝印 | 白色，每个连接器标注名称 + 引脚功能 |
| 安装孔 | 4× M2.5，四角 |

### 布局建议

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   [J1: EC11-L]     ┌──────────────────────┐    [J2: EC11-R] │
│    ○ 5P            │   ESP32-S3 DevKitC N16R8   │     ○ 5P        │
│                    │   (2×22 排母座 U1)   │                  │
│                    │                      │                  │
│                    │   70mm × 25mm        │                  │
│                    └──────────────────────┘                  │
│                                                              │
│   [J4: NFC]                                [J3: TOP MODULE]  │
│    ○ 7P                                     ○ 4P            │
│                                                              │
│                    [J5: USB-C ♀]                             │
│                     ═══════                                  │
│  (M2.5)                                           (M2.5)    │
│                    CosmoRadio V4 r1.0                        │
│  (M2.5)            dehonghao.com                   (M2.5)    │
└──────────────────────────────────────────────────────────────┘
                          80mm × 50mm
```

### 丝印标注

每个 JST 连接器旁标注：
- 接口名称：`EC11-L`, `EC11-R`, `TOP (BTN+LED)`, `NFC`
- 引脚功能：`V G A B S` / `V G S D` / `V G CK MO MI CS RS`
- 版本号：`CosmoRadio V4 r1.0`
- 网址/标识：`dehonghao.com`

---

## 配套线缆（预压 JST-XH 端子）

| 线缆 | 规格 | JST 端 | 外设端 | 数量/套 |
|------|------|--------|--------|--------|
| EC11 线缆 ×2 | 5P × 20cm | JST-XH 5P 公 | 直焊 EC11 引脚 | 2 |
| 顶部模块线缆 | 4P × 25cm | JST-XH 4P 公 | 焊到顶部子板 | 1 |
| NFC 线缆 | 7P × 15cm | JST-XH 7P 公 | 焊 RC522 排针 | 1 |
| USB-C 线缆 | 标准 C-to-C | 插 J5 | 插平板 | 1（外购） |

> 电子套件中：JST 端预压好，外设端预焊好，用户只需插 JST 到主板。

---

## 制造

| 项目 | 方案 | 成本 |
|------|------|------|
| PCB 打样 | JLCPCB，5 片起 | ¥2 + ¥20 运费 |
| PCB 量产 | JLCPCB，15 片 | ~¥45 含运 (¥3/片) |
| SMT 贴片 | 可选：电阻/电容用 0805 贴片，JLCPCB SMT 基础费 ¥8 | 省手焊时间 |
| 手焊部分 | 排母座 + JST 母座 + USB-C 座（直插件） | ~10min/板 |

### 生产流程

```
KiCad 设计完成
  → JLCPCB 下单（PCB + 可选 SMT 贴片电阻/电容）
  → 裸板到货（5-7 天）

焊接板载直插件（我们做，~10min/板）
  → 2×22 排母座 (U1)
  → JST-XH 母座 × 4 (J1-J4)
  → USB-C 母座 (J5)

预压 JST 线缆（我们做）
  → 4 根线缆/套 × 12 套 = 48 根

质检
  → 万用表通断
  → 插入 DevKitC，刷固件，逐一测试各端口

打包
  → PCB + DevKitC + 线缆 + 螺丝 = 1 套电子件
```

---

## 工具需求

| 工具 | 用途 |
|------|------|
| KiCad 9 | 原理图 + PCB layout |
| JLCPCB 插件 | 一键生成 Gerber + 下单 |
| 万用表 | 通断测试 |
| JST-XH 压线钳 | 预压端子线缆 |
| 烙铁 + 助焊剂 | 焊接直插件 |

---

## 待确认

- [ ] ESP32-S3 DevKitC N16R8 具体型号确认（N4 vs N4R2，是否需要 PSRAM）
- [ ] DevKitC 实际排针 pinout 核对 → 确定 PCB 排母座间距
- [ ] USB-C 走线：DevKitC 板载 USB-C 是否可直接引出 D+/D- 到 J5？还是需要从 GPIO19/20 走？
- [ ] NFC 模块安装位置：底板内侧 or 前面板背面？（影响线缆长度和天线读取距离）
- [ ] 顶部模块连接方式：单根 4P JST 是否够？或需要独立供电线？
- [ ] 是否让 JLCPCB 贴片 SMT 电阻/电容（省时 vs 加 ¥8 基础费）
