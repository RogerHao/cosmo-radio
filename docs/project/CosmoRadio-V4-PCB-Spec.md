# CosmoRadio V4 — PCB 载板规格 (v2.0)

> **版本**：v2.0（2026-05-18 整理，对应 PCB Plan v2）
> **范围**：技术方案验证 + 单套原型制作；JLCPCB 免费打样 5 片
> **设计文档**：详细执行步骤见 `~/.claude/plans/iridescent-snuggling-planet.md`
> **GPIO 权威表**：`/CLAUDE.md` "GPIO Pin Assignments"

---

## 设计理念

**Carrier Board（载板）** — ESP32-S3 DevKitC N16R8 通过两条 1×22 单排母插入 PCB，所有外设通过 5 路 XH2.54 JST 连接器即插即用。**整块板的本质是"连接器转接板 + EC11 RC 去抖网络"**——零有源元件，只有 8 个 0805 贴片被动件 + 6 个 THT 连接器。

### 为什么是 DevKitC N16R8 + carrier board

| 对比项 | ESP32-S3 SuperMini (V3) | ESP32-S3 DevKitC N16R8 (V4) |
|--------|------------------------|----------------------|
| USB HID | TinyUSB ✓ | TinyUSB ✓（同一套代码）|
| BLE | 有（未用）| 无（不需要）|
| GPIO 数量 | 11 可用 | 36+ 可用 |
| 价格 (1688) | ¥15 | ¥8-10 |
| 尺寸 | 25×18mm | 70×27mm |
| Flash / PSRAM | 4MB / 0 | 16MB / 8MB Octal |
| 固件迁移量 | — | 仅改 GPIO 编号 + sdkconfig |

ESP32-S3 有原生 USB OTG，TinyUSB HID 代码直接跑。GPIO 多 3 倍，方便加 NFC。N16R8 的 16MB Flash + 8MB PSRAM 足够 NFC 大缓冲。

### V3 问题 → V4 解决

| V3 问题 | V4 方案 |
|---------|---------|
| 飞线杂乱，杜邦线无锁扣 | PCB 走线 + JST-XH 带卡扣连接器 |
| 万能板空间局促 + 容易脱焊 | 100×100mm 双面 PCB，机械稳定 |
| 组装需焊接技能 | 全插拔 + 螺丝固定，附图文指南 |
| SuperMini GPIO 不够 | DevKitC 36+ GPIO，充裕 |
| 旋钮一格一格不准 | EC11 A/B 加 10kΩ + 10nF RC 去抖 |
| USB-C 母座焊接难 + 充电方案不稳 | OTG dongle (YK16-09E V1) 内置子板，主 PCB 不上 USB-C |

---

## 原理图

### GPIO 引脚分配

> 完整 GPIO → schematic net → 连接器 / PCB 位置 / PCB r1.0 walkaround，全部见 [/CLAUDE.md "GPIO Pin Assignments"](../../CLAUDE.md#gpio-pin-assignments-v4-pcb-r10--esp32-s3-devkitc-n16r8)。本 spec 不复述 GPIO 号，只描述**端子物理接线顺序**（生产视角，给装配 / 检验用）。

### 连接器定义（5 路 XH2.54，PCB r1.0）

| 连接器 | 类型 | 引脚顺序（pin 1 → pin N）| 线缆长度 | 方向 |
|--------|------|------|---------|------|
| J1 | XH2.54 2P 立式直针 | SIG, GND | 25cm | → Kailh BOX 按钮 |
| J2 | XH2.54 8P 立式直针 | CS, SCK, MOSI, MISO, IRQ, GND, VCC, RST | 15cm | → RC522 NFC mini |
| J3 | XH2.54 5P 立式直针 | A, GND, B, GND, SW | 20cm | → 左旋钮 (EC11-L) |
| J4 | XH2.54 5P 立式直针 | **SW, GND, B, GND, A** ⚠ 跟 J3 镜像 | 20cm | → 右旋钮 (EC11-R) |
| J5 | XH2.54 4P 立式直针 | VBUS, D-, D+, GND | <10cm | → OTG dongle USB-A 拆后 4 飞线 |

> **J2 引脚顺序与 RC522 mini 模块物理排针 1:1**，端子线直压无需交叉。详见 [docs/hardware/nfc.md](../hardware/nfc.md)。
> **J5 引脚顺序与 USB-A 公头物理引脚顺序一致**（VBUS, D-, D+, GND），方便 dongle 拆解后直焊。详见 [docs/hardware/otg-adapter.md](../hardware/otg-adapter.md)。
> **J4 镜像问题**：r1.0 原理图把 J4 画成 J3 的镜像，与 EC11 端子线常用顺序不兼容；固件层 walkaround，硬件 r1.1 应改回跟 J3 对称。详见 CLAUDE.md "PCB r1.0 EC11-R 接线 walkaround"。

### DevKitC 插座（两条 1×22 单排母）

| Ref | 类型 | 间距 |
|-----|------|------|
| U1A | 1×22P 排母 2.54mm 立式 | — |
| U1B | 1×22P 排母 2.54mm 立式 | — |
| U1A ↔ U1B 中心距 | **22.86mm** (= 9 × 2.54mm pitch) | breadboard 兼容 |

**为什么用两条单排母而非一条 2×22 双排母**：
- 单排母可独立对齐，安装时容差大、不会因双排"一边歪导致整体插不进"
- 单排零件 LCSC 可选范围更大，本身更便宜
- 焊接工艺更简单（每条 22 个焊点串行做完再做下一条）

### EC11 RC 去抖网络（关键设计）

每个 EC11 旋钮的 A/B 信号各加：
- **10kΩ 上拉电阻**到 3V3（增强边沿陡度，比内部 ~45kΩ 强 4.5×）
- **10nF 电容**对 GND（与上拉电阻形成 RC 低通滤波器，τ = 100µs）

**工作原理**：EC11 机械触点抖动通常在 µs 级；10nF 电容把高频抖动吸收掉，但用户手转的 ~10 detent/s 信号（频率 <100Hz）原封通过。这是 Ben Buxton 1998 年《Rotary encoders》经典文档里的标准接法。

**布局要求**：R/C 紧贴各自的 EC11 连接器（A/B pin 旁 ≤5mm），电容对 GND 走线尽量短。

```
EC11-L A 端 ──┬──── 10kΩ ──── 3V3
              ├──── 10nF ──── GND
              └─── 走线到 DevKitC EC11-L-A net

EC11-L B 端 ──┬──── 10kΩ ──── 3V3
              ├──── 10nF ──── GND
              └─── 走线到 DevKitC EC11-L-B net

(EC11-R 同结构，net 名换成 EC11-R-A / EC11-R-B；具体 GPIO 见 CLAUDE.md)
```

**不加去抖网络的 pin**：
- EC11 SW（按下事件慢，软件 debounce 足够）
- Action Button SIG（同上）
- NFC SPI 信号（模块板载已有去耦）

### 电路要点

1. **USB HID 通路**：DevKitC native USB → 排针 → J5 4P → dongle USB-A 拆后 4 飞线 → dongle 输出 Type-C plug → 平板
2. **供电**：外部充电器 → dongle Type-C 母座 → dongle 内部 CC IC 协商 → 平板 (充电) + dongle 输出 5V 给 DevKitC (USB 通路)
3. **3V3 来源**：DevKitC 板载 AMS1117-3.3 LDO
4. **EC11 上拉**：A/B 各 10kΩ 上拉到 3V3，A/B 各 10nF 对 GND（共 8 个 0805 贴片件）
5. **NFC SPI**：SPI2 经 GPIO Matrix（不走 FSPICLK 硬件 pin，因为那段在 octal PSRAM 占用范围内）
6. **板载 LED**：DevKitC 板载 WS2812B RGB，不外接

---

## PCB 物理规格

| 参数 | 值 |
|------|-----|
| 尺寸 | **100mm × 100mm**（JLCPCB 免费打样上限）|
| 层数 | 2 层 |
| 板厚 | 1.6mm |
| 铜厚 | 1oz |
| 表面处理 | HASL（无铅）|
| 阻焊 | 黑色 |
| 丝印 | 白色，每个连接器标注接口名 + 引脚功能 + **"OTG USB-C: DO NOT PLUG"** 警示 |
| 安装孔 | 4× M2.5（四角，孔径 2.7mm）|

### 布局建议

```
                ↑ BACK (远离用户, USB-C 朝外)
   ┌────────────────────────────────────────────┐
   │ ⊙ H1                                ⊙ H2  │
   │                                            │
   │            ┌──────────┐                    │
   │            │  J5 USB  │                    │
   │            │   4P     │                    │
   │            └──────────┘                    │
   │                                            │
   │  ┌──────┐ ┌──────┐                         │
   │  │ H-L  │ │ H-R  │            ┌────────┐   │
   │  │ 1×22 │ │ 1×22 │            │ J4     │   │
   │  │ sock │ │ sock │            │ EC11-R │   │
   │  │      │ │      │            │  5P    │   │
   │  │      │ │      │            └────────┘   │
   │  │      │ │      │                         │
   │  │      │ │      │       R3,R4,C3,C4       │
   │  │      │ │      │       (10k/10n ENC2)    │
   │  └──────┘ └──────┘                         │
   │                               ┌──────────┐ │
   │  ┌────────┐                   │ J2 NFC   │ │
   │  │ J3     │                   │   8P     │ │
   │  │ EC11-L │                   └──────────┘ │
   │  │  5P    │                                │
   │  └────────┘                                │
   │                                            │
   │  R1,R2,C1,C2 (10k/10n ENC1)                │
   │                                            │
   │  ┌────────┐                                │
   │  │ J1 BTN │                                │
   │  │   2P   │                                │
   │  └────────┘                                │
   │                                            │
   │   CosmoRadio V4 r1.0      OTG USB-C:       │
   │   dehonghao.com           DO NOT PLUG      │
   │ ⊙ H4                                ⊙ H3  │
   └────────────────────────────────────────────┘
                ↓ FRONT (朝向用户)
```

> 各连接器对应的 net / GPIO，见 CLAUDE.md "GPIO Pin Assignments"。

### 丝印标注

- 每个 JST 连接器旁标注：**接口名 + pin 顺序的信号名**（如 NFC 端子旁标 `NFC | CS SCK MOSI MISO IRQ G VCC RST`）
- 顶部丝印：`CosmoRadio V4 r1.0` + `dehonghao.com`
- **底部丝印警示**：`⚠ OTG USB-C: DO NOT PLUG`（必须，避免误插烧端口）
- 安装孔旁不丝印（避免遮挡螺丝）
- 所有丝印高 ≥ 1.0mm（JLC DFM 最小 0.8mm，留余量）

---

## 配套线缆（预压 JST-XH 端子）

| 线缆 | 规格 | JST 端 | 外设端 | 数量/套 |
|------|------|--------|--------|--------|
| EC11 线缆 ×2 | 5P × 20cm | JST-XH 5P 公 | 直焊 EC11 引脚 | 2 |
| Action Button 线缆 | 2P × 25cm | JST-XH 2P 公 | 焊到 Kailh BOX 轴 | 1 |
| NFC 线缆 | 8P × 15cm | JST-XH 8P 公 | 8P 排针压接到 RC522 | 1 |
| OTG dongle 线缆 | 4P × <10cm | JST-XH 4P 公 | 焊到 dongle 拆 USB-A 后的 4 焊盘 | 1 |
| dongle Type-C 输出 | Type-C plug + 短缆 | （dongle 自带）| 插入平板 | 1 |
| dongle Type-C 充电输入 | Type-C 母座 | （dongle 自带）| 接外部充电器 USB-C | 1 |

> 单套原型阶段：JST 端预压好，外设端预焊好，用户只需插 JST 到主板。

---

## 制造

| 项目 | 方案 | 成本 |
|------|------|------|
| PCB 打样 | JLCPCB 免费打样 5 片 | ¥0 + 国内运费 ~¥20 |
| SMT 贴片 | **不上**——单套原型，8 个 0805 手焊 ~5min | ¥0 |
| 手焊部分 | 2× 1×22 排母 + 5× JST 母座 + 8× 0805 (R+C) | ~15min/板 |

### 生产流程

```
KiCad 设计完成
  → JLCPCB 下单（仅打板，不贴片）
  → 5 片裸板到货（国内 ~3-5 天）

焊接元件（我们做，~15min/板）
  → R1-R4 (10kΩ 0805) × 4
  → C1-C4 (10nF 0805) × 4
  → H-L + H-R (1×22 排母) × 2
  → J1-J5 (XH2.54 母座) × 5

预压 JST 线缆（5 根）
  → EC11 5P × 2 / BTN 2P × 1 / NFC 8P × 1 / OTG dongle 4P × 1

质检
  → 万用表通断（5V/3V3/GND 到 J 端子和 H-L/H-R 各 pin）
  → 插入 DevKitC，UART USB-C 烧固件，逐一测试各端口
  → 旋钮转动测试（验证 RC 去抖效果，每格一个事件）

打包
  → PCB + DevKitC + dongle + 线缆 + 螺丝 = 1 套电子件
```

---

## 工具需求

| 工具 | 用途 |
|------|------|
| JLCEDA Pro | PCB r1.0 实际使用的原理图 + layout 工具（源文件 `~/Downloads/P1.epro2`） |
| KiCad 10+ | Plan v2 备选方案，保留在 `pcb/cosmoradio-v4-carrier/` 作历史参考 |
| JLCPCB | 打板下单 |
| 万用表 | 通断测试 |
| JST-XH 压线钳 | 预压端子线缆 |
| 烙铁 + 助焊剂 + 加热台 | 焊接 0805 + 直插件 |

---

## 设计追溯

- 完整设计计划 + 风险评估：`~/.claude/plans/iridescent-snuggling-planet.md`
- DevKitC 物理引脚分布权威表：`/CLAUDE.md` + `pcb/asset/ESP32-S3-DevKitC-N16R8.png`
- OTG dongle 集成方案：`docs/hardware/otg-adapter.md`
- NFC 模块接口：`docs/hardware/nfc.md`
- 飞线万能板实证：`pcb/asset/飞线测试版本-顶视图.JPG` + `飞线测试版本-飞线图.JPG`

---

## 历史归档

v1.0（2026-04-01 出稿，对应 V4 启动期）已被本 v2.0 取代。v1.0 的主要差异：
- 板尺寸 80×50mm → 100×100mm
- 连接器 4 路 → 5 路（新增 J5 OTG USB）
- 包含 USB-C 母座 J5/J6 → 移除（OTG dongle 接管充电）
- 包含 SS34 防倒灌 + 4× 5.1k CC 下拉 → 全移除
- 包含 LED 4P 顶部模块连接器 → 移除（用 DevKitC GPIO48 板载 RGB）
- GPIO 编号是 V3 残留（顺序 1-8）→ 更新为 V4 终版（与固件实测一致）
- 单排母 vs 双排母未定 → **确定两条 1×22 单排母**
- 旋钮稳定性未涉及 → **新增 10kΩ + 10nF RC 去抖网络**

如需查阅 v1.0 内容，请 `git log -- docs/project/CosmoRadio-V4-PCB-Spec.md`。
