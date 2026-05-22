# CosmoRadio V4 — NFC 模块方案

> 用户将带有 NFC/RFID 标签的实物放置在收音机上方，触发安卓平板上的对应行为。
> ESP32 作为 USB HID 键盘，通过"键入字符串"方式将 NFC 事件传递给平板。

> ✅ **NFC 数量已确认（2026-05-06）**：1 个 RC522 mini，最终方案，不再讨论。下文"双 NFC 方案备选"段保留作为备忘，不再实施。

---

## 1. 硬件方案

### 1.1 模块选型：RC522 Mini

| 参数 | 值 |
|------|------|
| IC | NXP MFRC522 |
| 工作频率 | 13.56 MHz (ISO/IEC 14443 Type A) |
| 通信接口 | SPI (Mode 0, max 10 Mbps) |
| 工作电压 | 2.5V — 3.3V |
| 工作电流 | 13 — 26 mA (激活), < 80 µA (休眠) |
| 模块尺寸 | 30 × 40 mm (mini 版) |
| 单价 | ¥9.94 |

支持标签类型：MIFARE Classic 1K/4K, MIFARE Ultralight, **NTAG213/215/216**。

### 1.2 推荐标签：NTAG215

| 规格 | NTAG213 | **NTAG215** | NTAG216 |
|------|---------|-------------|---------|
| UID 长度 | 7 bytes | 7 bytes | 7 bytes |
| 用户存储 | 144 bytes | 504 bytes | 888 bytes |
| 写入寿命 | 50,000 次 | 200,000 次 | 500,000 次 |
| 密码保护 | PWD_AUTH | PWD_AUTH | PWD_AUTH |

选 NTAG215 的理由：
- 504 bytes 用户存储，够写入自定义 payload（UID 之外的元数据）
- Amiibo 标准标签，量产最大、价格最低（¥0.3 — ¥1/片）
- 如果只用 UID 识别不写数据，NTAG213 也够用

### 1.3 接线 (NFC 8P 端子 — JST-XH, **PCB 右侧底**)

NFC 8P 端子在 PCB 上的物理位置：**用户右侧底部**。与 RC522 mini (MF522-MINI) 模块的 8-pin 排针**物理顺序一一对应**，8P 端子线直接压接即可，无需交叉或空线。

> 引脚顺序来源：zave 旗舰店商品详情图实拍丝印，从排针第 1 脚到第 8 脚。
>
> ![RC522 mini 引脚定义与原理图](../assets/rc522-mini-pinout-and-schematic.png)

| Pin | 模块丝印 | 信号 | SPI 总线 | 说明 |
|-----|---------|------|---------|------|
| 1 | SDA  | CS (片选) | SPI2 (软件 CS) | 软件控制，低有效 |
| 2 | SCK  | SPI 时钟  | SPI2 CLK | |
| 3 | MOSI | 主→从数据 | SPI2 MOSI | |
| 4 | MISO | 从→主数据 | SPI2 MISO | |
| 5 | IRQ  | 中断请求  | — | 低有效，固件未使用，仅接线预留 |
| 6 | GND  | 地       | — | |
| 7 | RST  | 复位     | — | 低有效 |
| 8 | 3V3  | VCC      | — | 模块供电 |

> 各信号对应的 GPIO 号、连接器编号 / 物理位置，见 CLAUDE.md "GPIO Pin Assignments"。

使用 SPI2 (FSPI) 硬件 SPI 总线，信号走 GPIO Matrix（不能用 FSPI IO MUX 直连，因为那段引脚在 N16R8 内部 octal PSRAM 占用范围内）。RC522 工作频率 ~8 MHz，远低于 GPIO Matrix 上限（80 MHz），信号完整性无压力。

> **IRQ 引脚说明**：MFRC522 检测到标签进入射频场时拉低 IRQ。当前 `abobija/rc522` 库采用 SPI 轮询模式，不依赖 IRQ，但保留接线方便未来切换中断驱动模式以降低 CPU 占用。
>
> **注意**：模块丝印 RET 实际为 RST（复位），系厂商丝印错误。

### 1.4 安装位置

RC522 天线安装在**外壳顶部模块内侧**（朝上），用户将标签物品放置在收音机顶部。

> **双 NFC 方案备选**：若确认 2 个 NFC，一个放顶部模块左侧、一个放右侧。第二个 RC522 需要独立 CS 引脚（从 CLAUDE.md "Reserved" 列表挑），共享 FSPI 总线的 SCK/MOSI/MISO；RST 和 IRQ 也各需独立 GPIO。总计额外占用 3 个 GPIO（CS + RST + IRQ）。

关键约束：
- **PLA/PETG 对 13.56 MHz 磁场透明** — 2mm 壁厚几乎无衰减，读取距离不受影响
- **金属是大敌** — 天线区域附近不能有铜箔、螺丝、USB 屏蔽层。PCB 载板的地平面需要在天线投影区域开窗
- Mini 模块天线较小，有效读取距离约 15 — 30 mm（贴片标签）/ 30 — 50 mm（卡片标签）
- 标签穿过 2mm PLA 外壳后，实际工作距离 ≈ **13 — 28 mm**（贴片）

```
  ┌──────────────────────────────┐
  │     NFC 标签物品放置区域       │  ← 用户将物品放在这里
  ├──────────────────────────────┤
  │  ░░░░ 3D 打印顶部模块 ░░░░   │  ← 2mm PLA 壁
  │  ┌────────────────────────┐  │
  │  │   RC522 Mini 天线面朝上  │  │  ← 模块贴装在顶盖内侧
  │  └────────────────────────┘  │
  ├──────────────────────────────┤
  │       ESP32-S3 载板区域       │
  └──────────────────────────────┘
```

---

## 2. 通信协议设计

### 2.1 核心思路

ESP32 始终作为 USB HID 键盘。NFC 事件通过**键入结构化字符串**的方式传递给安卓平板，平板端应用解析该字符串并执行对应动作。

这种方案的优势：
- ESP32 固件不需要维护 UID → 动作的映射表
- 新增标签不需要刷固件，只需在安卓端配置
- 标准 HID 键盘协议，无需自定义驱动
- 调试方便 — 任何文本编辑器都能看到输出

### 2.2 NFC HID 报文格式（**2026-05-07 定稿**）

> **协议演进**：早期方案是单一的 `NFC:<UID>\n`，但 UID 不可控（厂家烧录、每张卡随机），意味着每张卡都需要"逐张录入服务端数据库"才能上线。**当前方案改为读取卡上 NDEF Text Record 内容作为产品 ID**，UID 退化为兜底诊断信号。

固件按以下两条路径之一键入：

| 卡片状态 | HID 输出 | 说明 |
|---------|---------|------|
| 卡上写有 NDEF Text Record | `#<payload>\n` | **主路径** — payload 由生产时写入（如 `R01`、`track-42`） |
| 空白卡 / 非 NDEF / 解析失败 | `NFC:<UID_HEX>\n` | 兜底 — 方便诊断"这张卡是不是没写过内容" |

示例：
```
#R01\n                     ← 用户写入了 "R01"，平板查映射表决定播什么
#track-42\n                ← 用户写入了 "track-42"
#112358\n                  ← 用户写入了 "112358"（实测）
NFC:04734D67220289\n       ← 同一张卡如果擦除了 NDEF，回退为 UID
```

平板侧解析（伪代码）：

```kotlin
when (line.firstOrNull()) {
    '#'  -> handleNfcPayload(line.drop(1))  // "R01" → 查映射 → 执行
    'N'  -> handleNfcUid(line.removePrefix("NFC:"))  // 兜底，提示用户卡未配置
    else -> /* keyboard event from buttons/encoders */
}
```

### 2.2.1 Payload 编码契约

| 项目 | 约束 |
|------|------|
| 字符集 | `[A-Za-z0-9 # : / . - _]`（HID 表能直接键入的） |
| 长度上限 | 32 字节（超长截断；建议 ≤ 12 字节减少键入延迟） |
| 编码 | UTF-8 NDEF Text Record（status 字节 lang_len 任意） |
| 写入工具 | 任一手机 NFC writer app（NFC Tools 等） |
| 不支持 | 中文 / 空格 / UTF-16 / URI 类型 / 多记录 NDEF |

### 2.2.2 同卡去重

固件维护"上次触发的 UID + 时间戳"。同 UID 在 1500ms 内重复出现视为 RC522 心跳抖动（卡片未真正离开），静默丢弃。这意味着用户**必须把卡拿开 ≥1.5s 再放回**才能触发第二次。

### 2.2.3 历史方案（仅 UID）— 已废弃

```
NFC:<UID_HEX>\n   ← 14 字符 UID，留作兜底
```

废弃原因：UID 厂家随机烧录，新卡上线必须逐张录入服务端，损坏一张卡需改服务端配置。NDEF Text 方案让产品身份与卡片物理身份解耦。

### 2.3 键入速率

HID 键盘的轮询间隔为 10ms（当前描述符配置）。每个字符需要 key-down + key-up 两个报文，最快 20ms/字符。

19 个字符 × 20ms = **~380ms** 完成一次 NFC 报文传输。

实际建议字符间隔 5ms（key-down 后 5ms key-up，再 5ms 下一个 key-down），总耗时约 **~190ms**，用户感知为即时响应。

### 2.4 协议扩展（可选）

后续如果需要传递更多信息，可扩展格式：

```
NFC:<UID_HEX>:<ACTION>\n     ← 带动作码
NFC:04A1B2C3D4E5F6:R\n       ← R = 标签放置 (Read)
NFC:04A1B2C3D4E5F6:X\n       ← X = 标签移除 (eXit)
```

当前 V4 阶段只实现基础 `NFC:<UID>\n`，移除检测和扩展动作留作后续。

### 2.5 与现有按键的兼容性

NFC 字符串键入会占用 HID 报文通道。需要处理的边界情况：

| 场景 | 处理方式 |
|------|---------|
| NFC 键入期间旋钮转动 | 队列缓冲，NFC 字符串完成后再发送旋钮事件 |
| NFC 键入期间按钮按下 | 按钮事件进队列，NFC 完成后发送 |
| 连续放置同一标签 | 去重 — 同一 UID 在移除前不重复发送 |
| 连续放置不同标签 | 立即发送新 UID（中断当前如果还在键入） |

实现方式：NFC 键入在独立 FreeRTOS task 中执行，通过 mutex 与 `on_input_event()` 互斥访问 HID 报文接口。

---

## 3. 软件实现

### 3.1 驱动库

使用 [`abobija/esp-idf-rc522`](https://github.com/abobija/esp-idf-rc522) (v3.4.3)：

- ESP Component Registry 可用：`idf.py add-dependency "abobija/rc522"`
- ESP-IDF ≥ 5.0，支持 SPI / I2C
- 事件驱动：卡片检测 → 激活 → 空闲 → 移除
- NTAG 支持：`GET_VERSION`, `READ`, `FAST_READ`, `WRITE`, `PWD_AUTH`
- 软件 CS 控制（不占用硬件 CS 资源）

### 3.2 SPI 配置

```c
rc522_spi_config_t driver_config = {
    .host_id = SPI2_HOST,  // FSPI on ESP32-S3
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = 37,   // FSPIQ
        .mosi_io_num = 35,   // FSPID
        .sclk_io_num = 36,   // FSPICLK
    },
    .dev_config = {
        .spics_io_num = 34,  // FSPICS0 (软件控制)
    },
    .rst_io_num = 9,
};
```

ESP32-S3 SPI2 (FSPI) 通过 GPIO Matrix 灵活映射，不存在半双工限制问题。RC522 工作频率 ~8 MHz，GPIO Matrix 完全够用。

### 3.3 模块架构

新增两个文件到 `tusb_hid/main/`：

```
tusb_hid/main/
├── tusb_hid_example_main.c   # 现有：入口 + HID + 事件分发
├── input_handler.c/h         # 现有：GPIO 中断输入
├── led_indicator.c/h         # 现有：WS2812 LED
├── nfc_reader.c              # 新增：RC522 驱动 + UID 读取
└── nfc_reader.h              # 新增：NFC 接口定义
```

### 3.4 接口设计

```c
// nfc_reader.h

typedef struct {
    uint8_t uid[7];       // NTAG 7-byte UID
    uint8_t uid_len;      // 实际 UID 长度 (4 or 7)
    bool present;         // 标签是否在场
} nfc_event_t;

typedef void (*nfc_event_cb_t)(const nfc_event_t *event);

esp_err_t nfc_reader_init(void);
void nfc_reader_set_callback(nfc_event_cb_t cb);
esp_err_t nfc_reader_start(void);
esp_err_t nfc_reader_stop(void);
```

### 3.5 HID 字符串键入实现

HID 键盘发送可打印字符需要查表 ASCII → HID keycode + modifier：

| 字符 | HID Keycode | Modifier |
|------|-------------|----------|
| `N` | 0x11 | Left Shift |
| `F` | 0x09 | Left Shift |
| `C` | 0x06 | Left Shift |
| `:` | 0x33 | Left Shift |
| `0`—`9` | 0x27, 0x1E—0x26 | 无 |
| `A`—`F` | 0x04—0x09 | Left Shift |
| Enter | 0x28 | 无 |

实现为通用的 `hid_type_string(const char *str)` 函数，可复用于未来其他字符串传输需求。

### 3.6 事件流

```
RC522 硬件中断 / 轮询
       │
       ▼
abobija/rc522 事件回调
  RC522_EVENT_PICC_STATE_CHANGED
       │
       ├─ state == ACTIVE  → 读取 UID → nfc_event_cb(present=true)
       └─ state == IDLE    → nfc_event_cb(present=false)
       │
       ▼
tusb_hid_example_main.c: on_nfc_event()
       │
       ├─ 获取 HID mutex
       ├─ hid_type_string("NFC:04A1B2C3D4E5F6")
       ├─ send_key_pulse(KEY_ENTER)
       ├─ 释放 HID mutex
       └─ LED 闪烁反馈 (紫色)
```

### 3.7 去重逻辑

```c
static uint8_t last_uid[7] = {0};
static bool tag_present = false;

void on_nfc_event(const nfc_event_t *event) {
    if (event->present) {
        if (tag_present && memcmp(last_uid, event->uid, 7) == 0) {
            return;  // 同一标签仍在场，跳过
        }
        memcpy(last_uid, event->uid, 7);
        tag_present = true;
        // 发送 HID 字符串
    } else {
        tag_present = false;
        memset(last_uid, 0, 7);
        // 可选：发送移除事件
    }
}
```

---

## 4. 安卓端集成要点

平板端应用需要：

1. **监听键盘输入** — 在前台 Activity 中捕获 `dispatchKeyEvent()` 或维护一个隐藏的 `EditText`
2. **解析 NFC 前缀** — 识别 `NFC:` 开头的字符串，提取 UID
3. **UID → 动作映射** — 本地维护一张映射表（JSON / SQLite），将 UID 映射到具体行为（切换频道、播放音效、触发动画等）
4. **标签注册流程** — 提供 UI 让用户"扫描新标签 → 命名 → 绑定动作"

映射表示例：
```json
{
  "04A1B2C3D4E5F6": { "action": "channel", "value": "jazz" },
  "04D8E9F0A1B2C3": { "action": "channel", "value": "ambient" },
  "0412345678ABCD": { "action": "effect", "value": "static" }
}
```

---

## 5. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| RC522 读取距离不够（物品标签贴在非最优位置） | 用户体验差 | 标记推荐放置区域；必要时换 40×60mm 标准模块 |
| HID 字符串键入被安卓输入法拦截 | 字符丢失或变形 | 安卓端设置硬件键盘模式，禁用软键盘 |
| NFC 字符串键入与旋钮事件冲突 | HID 报文错乱 | mutex 互斥 + 事件队列 |
| FSPI 总线与其他外设冲突 | SPI 通信失败 | V4 中 FSPI 专用于 NFC，无其他 SPI 设备 |
| 多标签同时在场 | 只能识别一个 | RC522 单天线单标签限制，可接受 |

---

## 6. 实施步骤

| 步骤 | 内容 | 依赖 |
|------|------|------|
| 1 | `idf.py add-dependency "abobija/rc522"` | 无 |
| 2 | 实现 `nfc_reader.c/h` — SPI 初始化 + UID 读取 | RC522 模块到货 |
| 3 | 实现 `hid_type_string()` — ASCII → HID keycode 查表 + 逐字符发送 | 无 |
| 4 | 集成到 `tusb_hid_example_main.c` — 注册回调 + mutex | 步骤 2, 3 |
| 5 | PCB NFC 端子焊接 + 飞线测试 | PCB 到货 |
| 6 | 读取距离测试（穿透 2mm PLA） | 步骤 2 + 外壳样件 |
| 7 | 安卓端字符串解析 + 映射表 | 步骤 4 |
| 8 | 端到端联调：标签 → ESP32 → HID → 平板 → 动作 | 全部 |
