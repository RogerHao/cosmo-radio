# CosmoRadio V4 — 技术方案验证报告

> 交付对象：杨炜乐（Putopia / 上海噗噗噗教育科技有限公司）/ Arthur（纪晟）
> 服务方：Hao Lab（惜忆智能(深圳)有限公司）
> 范围：技术方案验证 + 单套工作原型 | 合同价：¥5,000 | 重启协议：2026-04-29
> 验证完成：2026-05-22（PCB r1.0 实焊 + 双板并排功能验证通过）

---

## 1. 结论摘要

CosmoRadio V4 的完整硬件交互方案——**安卓平板 + 定制 ESP32-S3 载板（USB HID）+ 双 EC11 旋钮 + 机械 Action Button + RC522 NFC**——已在定制 PCB（r1.0）上端到端验证通过，并装配出一套可演示的工作原型。

所有方案级技术风险均已收敛，无遗留的"原理是否成立"未知项。剩余事项为可量产化的工程优化（见 §6）和一项需新平板到货后补做的回归测试（§5）。

| 子系统 | 验证状态 |
|--------|----------|
| 定制 PCB 载板（r1.0） | ✅ 打样 + 实焊 + 功能验证通过 |
| USB HID（平板识别 ESP32 为键盘） | ✅ 通过 |
| USB Host + 充电同时工作 | ✅ 通过（成品 OTG dongle 内置方案） |
| Action Button（机械轴 + 卫星轴） | ✅ 信号通过；手感待新平板装配复测 |
| 双 EC11 旋钮（旋转 + 按压） | ✅ 通过（右旋钮经固件 walkaround） |
| RC522 NFC（读 NDEF + UID） | ✅ 通过 |
| 固件 V4（HID 编码 + 多键并发） | ✅ 通过 |

---

## 2. 系统架构

```
Android 平板 (Galaxy Tab A9 8.7")
   │  USB HID（标准键盘）
   ▼
ESP32-S3 DevKitC N16R8  ←插在→  定制载板 PCB r1.0
   │ GPIO / SPI
   ├── EC11 旋钮 ×2（频率 / 模式）
   ├── Kailh BOX 轴 + 6.25U 卫星轴 Action Button
   ├── RC522 mini NFC（13.56MHz SPI）
   └── DevKitC 板载 GPIO48 RGB LED（状态指示）

充电器 ──→ 内置 OTG dongle 子板（YK16-09E V1 拆解）──→ 平板（Host + 充电同时）
```

控制器作为标准 USB 键盘连接平板，所有输入以模拟按键传递，平板侧 Web 应用解析。无需安装驱动、无需配对。

---

## 3. 各子系统验证记录

### 3.1 定制 PCB 载板（核心交付）

- **形态**：100×100mm 双面板，ESP32-S3 DevKitC N16R8 通过两条 1×22 单排母插入（不焊死，易更换），5 路 XH2.54 连接器即插即用，消灭 V3 的万能板飞线。
- **被动网络**：EC11 A/B 信号各加 10kΩ 上拉 + 10nF 对地 RC 去抖（τ=100µs），解决飞线版"一格不准"问题。
- **设计源**：嘉立创EDA专业版手绘，r1.0。源文件与制造导出清单见 `pcb/cosmoradio-v4-jlceda/`。
- **验证**：打样 5 片，焊接 2 片并排测试，全链路功能通过。

### 3.2 USB HID + 充电同时工作

- ESP32-S3 原生 USB OTG（GPIO19/20）+ TinyUSB，平板识别为标准键盘，即插即用。
- **充电难点**：平板需同时为 USB **Data Host**（驱动 ESP32）+ **Power Sink**（被充电），非 USB-C 默认角色组合。
- **方案**：拆解成品一分二 OTG + 充电线材（YK16-09E V1），取内部 PCB 作内置子板，经 J5 接入。实测平板可在 HID 输入的同时充电，HID 不断连。
- **失败路径记录（已排除）**：被动 SS34 + 6P 转接板注入 VBUS（2026-04-30 实测）——无法完成 Type-C/PD 角色协商，平板不充电。详见 `pcb/research/Tab_A9_OTG_Charging_Feasibility_Report.md`。

### 3.3 Action Button（机械按钮机构）

- 方案：Kailh BOX 轴 + KEYCOOL 6.25U 卫星轴空格键（~10cm），借鉴机械键盘蝴蝶结构，解决 V3 易卡、行程不足、手感弱。
- 信号：维持 0/1 数字信号，模拟回车键。
- 状态：电气信号验证通过；**最终手感需在新 Tab A9 到货后随外壳装配复测**（原 R4 测试平板已损坏，当前用安卓手机替代）。

### 3.4 双 EC11 旋钮

- 每个旋钮 A/B/SW 三信号，纯机械开关（3 IO + 共地，无需 VCC）。
- 左旋钮（J3）→ 上/下方向键（调频）；右旋钮（J4）→ 左/右方向键（切换模式）；按压 → F1/F2 占位。
- **右旋钮已知 issue + 修复**：r1.0 原理图把 J4 引脚顺序画成跟 J3 镜像，导致物理线序错位（表现为"任意方向转都是 CW、按下变 CCW"）。**固件层 walkaround 已修复**（`main/input_handler.c` 重指派 GPIO + swap A↔B 让左右旋钮"向内拨"方向一致），**硬件无需返工**。

### 3.5 RC522 NFC

- 模块：RC522 mini（SPI），标签：NTAG21x。
- 固件读卡上写入的 **NDEF Text Record** 作为产品 ID（与不可控的卡 UID 解耦），HID 原样键入 payload + Enter；空白卡/解析失败时键入 `NFC:<UID>` 兜底。
- 同 UID 1.5s 去重，应对 RC522 心跳抖动。
- 端到端验证：iPhone NFC Tools 写卡 → ESP32 读到字节级一致的 UID + payload → HID 键入。
- SPI 时钟当前 1MHz（飞线版 RF 噪声限制），PCB 版可尝试拉回 5MHz。

### 3.6 固件 V4

- ESP-IDF + TinyUSB，从 V3 SuperMini pinout 完整迁移到 DevKitC N16R8。
- HID Key Report **多键并发**（`pressed_keys[6]` + mutex），NFC 字符串注入与用户按键无竞态。
- NFC 失败降级：无 RC522 时只 warning 不 abort，裸板演示不 reboot loop。
- sdkconfig 升级 N16R8（16MB QIO flash + 8MB octal PSRAM @ 80MHz）。

---

## 4. HID 交互协议（平板侧对接）

| 输入 | HID 输出 | 功能 |
|------|----------|------|
| Action Button | Enter | 确认 |
| 左旋钮 旋转 | 上/下方向键 | 调频 |
| 右旋钮 旋转 | 左/右方向键 | 切换模式 |
| 左/右旋钮 按压 | F1 / F2 | 占位，平板可重映射 |
| NFC 卡（写了 NDEF Text） | `<payload>\n` | 主路径，原样键入 |
| NFC 卡（空白/非 NDEF） | `NFC:<UID_HEX>\n` | 兜底 |

NFC 写卡用任一手机 NFC writer app（NFC Tools 等），固件零工具依赖。payload 字符集 `[A-Za-z0-9 # : / . - _]`，≤32 字节。

---

## 5. 已知限制 / 待回归

| 项 | 状态 | 处理 |
|----|------|------|
| Action Button 最终手感 | 待复测 | 新 Tab A9 到货后随外壳装配验证 |
| 整机热稳定 + 2h 充电链路 | 待复测 | 同上（当前在安卓手机上验证） |
| NFC SPI 时钟 1MHz | 可优化 | PCB 版铜箔走线 + 去耦后尝试 5MHz |
| NFC 穿透壁厚读距 | 待测 | 外壳打印后测（壁厚 2mm） |

---

## 6. r1.1 优化建议（量产化方向，非本次范围）

1. **J4 改对称**：把 EC11-R 连接器引脚顺序改回跟 J3 一致 `[A, GND, B, GND, SW]`，去掉固件 walkaround。
2. **原理图标签修正**：H-L pin1 / H-R pin1·pin2 的 GND/+5V 误标（r1.0 因走线未真连而幸免）。
3. **NFC 5MHz**：铜箔走线后拉回 SPI 全速。
4. 批量化后单套电路成本可从原型的 ~¥134 降至 ~¥80-90（PCB 打样分摊 + dongle 整件成本是原型阶段的主要溢价）。

---

## 7. 交付物清单

| 类别 | 内容 | 位置 |
|------|------|------|
| 固件源码 | ESP-IDF 工程（USB HID + NFC + 输入处理） | 仓库根 / `main/` |
| PCB 设计源 | 嘉立创EDA r1.0 工程 + 原理图渲染 | `pcb/cosmoradio-v4-jlceda/` |
| PCB 制造文件 | Gerber/钻孔/坐标/BOM/PDF/3D | `pcb/.../manufacturing/`（待 GUI 导出，清单见 `pcb/README.md`） |
| 3D 模型 | V4 外壳 + 结构件全套（STL + Bambu 工程） | `models/v4/` |
| BOM 与成本 | 单套原型物料清单 + 报价分析 | `docs/project/CosmoRadio-V4-BOM及报价分析.md` |
| 硬件方案文档 | NFC / USB / OTG 适配器设计 | `docs/hardware/` |
| HID 测试工具 | 三圆布局网页测试器 | `test/hid-test.html` |
| 工作原型 | 1 套实焊验证 PCB + 装配原型 | 实物 |

> GPIO 引脚分配、NFC 编码契约等技术细节的唯一权威源为仓库根 `CLAUDE.md`。
