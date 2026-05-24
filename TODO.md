# CosmoRadio V4 — TODO

> **✅ 技术验证 + 单套原型已交付（2026-05-22）** — 见 [技术验证报告](docs/project/CosmoRadio-V4-技术验证报告.md)。
> 剩余仅为 post-delivery 回归（需新 Tab A9）+ r1.1 优化，详见文末「当前范围」与验证报告 §5/§6。
>
> 第四期：2026-04-01 启动 → 2026-04-02 取消 → 2026-04-29 重启
> 当前交付：技术方案验证 + 单套原型制作
> 项目文档：[docs/project/](docs/project/)（symlink → codex）
> 合同价：¥5,000 | 目标交付：2026-05-20 左右（实际 2026-05-22 完成）

## 2026-05-07 整体技术方案验证完成 ✅

电气、固件、NFC 协议在万能板飞线版上**端到端跑通**：

- ESP32-S3 DevKitC N16R8 主控 + 4 路 XH2.54 连接器（双 EC11、Action Button、NFC）（注：飞线版当时 J 编号跟 r1.0 PCB 不同，详见 CLAUDE.md）
- 拆解一分二 OTG + 充电线材内置子板，平板 USB Host + 充电同时工作
- 固件 V4 GPIO 全部跑通：Action Button、双 EC11 旋转 + 按压、板载 RGB
- NFC 链路：RC522 SPI（1MHz）→ 读 NTAG215 UID + NDEF Text Record → HID 键入 `#<payload>\n`
- 协议契约：NDEF Text 主路径 / `NFC:<UID>` 兜底 / 1.5s 同卡去重
- 端到端独立验证：iPhone NFC Tools 写入 "112358" → ESP32 读到字节级一致 UID + payload

**剩余工作集中在 PCB 设计 + 结构件 + 整机装配**，不再有方案级未知风险。

---

## 2026-05-08 今日双线并行 + PCB 前最后一项验证

| 线 | 谁 | 内容 |
|----|----|----|
| 固件 | Claude | 彻底优化升级到 V4（A 类技术债清理 + 代码整理） |
| 结构 | Dehong | NFC 模块 + 长按条按钮模块结构设计 + 历史结构改进 |

✅ **2026-05-18 PCB Plan v2 锁定**：USB 互联走线方案确定为"OTG dongle 拆 USB-A → 4 飞线 → J5 4P → DevKitC 排针"，详见 `~/.claude/plans/iridescent-snuggling-planet.md`。

---

## 2026-05-22 PCB r1.0 实焊 + 固件验证完成 ✅

最终走 JLCEDA Pro 手绘 r1.0，验证版工程已归档 `pcb/cosmoradio-v4-jlceda/`。早期 KiCad Plan v2 备选未使用，已删除（见 git 历史）。

- ✅ PCB 打板 + 焊接 + DevKitC 插入 + JST 端子线压接
- ✅ 双 EC11 + Action Button + NFC + USB HID 全链路验证通过（两台板子并排测）
- ✅ EC11-R 右旋钮初次表现异常（任意方向都是 CW、按下变 CCW）——根因：J4 引脚顺序在原理图里画成跟 J3 镜像，物理 EC11 端子线序错位
- ✅ 固件层 walkaround：`main/input_handler.c` 重新指派 `GPIO_ENC2_A/B/SW`，并 swap A↔B 让两个旋钮"向内拨"方向一致——硬件不返工
- 📌 PCB r1.1（未启动）：把 J4 改成跟 J3 对称的 `[A, GND, B, GND, SW]`，去掉固件 walkaround
- 📌 顺便把原理图里几个标错的"GND/+5V"标签（H-L pin 1、H-R pin 1/pin 2）改正——这些 net 物理上不是电源，r1.0 是因 PCB 走线没真连才幸免

详见 [CLAUDE.md "PCB r1.0 EC11-R 接线 walkaround"](CLAUDE.md#pcb-r10-ec11-r-接线-walkaround2026-05-22)。

---

## 当前范围（2026-04-29 重启）

- [x] 与 Arthur 达成重启协议：V4 缩小为技术方案验证和单套原型制作
- [x] 合同价确认：¥5,000
- [x] 目标交付时间确认：2026-05-20 左右
- [x] **验证 PCB 载板设计路径** ✅ 2026-05-07（飞线版跑通，剩 KiCad 落版）
- [x] 完成一套可演示原型，不再按 4 套整机 + 8 套电子套件交付 ✅ 2026-05-22

---

## 关键待办

### USB 互联走线 ✅ 2026-05-18 已锁定（Plan v2）

> 方案：OTG dongle (YK16-09E V1) 拆 USB-A 母座 → 4 根 AWG28 飞线 → J5 (XH2.54 4P) → DevKitC 排针 GPIO19/20 + 5V/GND。
> 详见 [`docs/hardware/otg-adapter.md`](docs/hardware/otg-adapter.md) + `~/.claude/plans/iridescent-snuggling-planet.md`。

- [x] OTG 子板选型 (YK16-09E V1) ✅ 2026-05-06
- [x] USB 走线方案确认（J5 4P → DevKitC 排针，无中间 USB-C 跳线）✅ 2026-05-18

### PCB 载板设计 ✅ 2026-05-22 r1.0 实焊验证完成

> 最终路径：JLCEDA Pro 手绘 r1.0，验证版工程 `pcb/cosmoradio-v4-jlceda/`。KiCad Plan v2 备选未使用，已删除（git 历史可追溯）。

- [x] DevKitC N16R8 排针间距确认（22.86mm = 9 × 2.54mm pitch）✅ 2026-05-18
- [x] 用两条 1×22 单排母（不用 2×22 双排）✅ 2026-05-18
- [x] 加 EC11 RC 去抖网络（4× 10kΩ + 4× 10nF 0805）✅ 2026-05-18
- [x] Plan v2 KiCad 备选方案（circuit-synth + pcbnew API + Freerouting，Gerber + BOM + 渲染图全套）✅ 2026-05-18 — 未上 JLCPCB，被 P1.epro2 替代
- [x] JLCEDA Pro 手绘 r1.0 原理图 ✅
- [x] r1.0 PCB 打样 + 焊接 ✅ 2026-05-22
- [x] r1.0 双板并排功能验证 ✅ 2026-05-22
- [ ] **PCB r1.1**：修正 J4 镜像、修正 H-L pin 1 / H-R pin 1·pin 2 的 GND/+5V 误标，并去掉固件 walkaround

### USB-C 板载集成评估 ✅ 2026-05-18 关闭 — OTG dongle 方案替代
> ~~原方案 B：PCB 双 USB-C 座 + 被动 SS34 VBUS 注入~~ → 2026-04-30 实测失败（Tab A9 PD 协议不接受）
> **最终方案**：外购 OTG dongle (YK16-09E V1) 拆 USB-A 内置，详见 [docs/hardware/otg-adapter.md](docs/hardware/otg-adapter.md)
> ~~历史选型详情~~ → [docs/hardware/usb.md](docs/hardware/usb.md) (已标记历史文档)

- [x] 研究 USB-C 母座直焊 PCB 方案（CC 引脚配置、ESD 保护）→ 鹏宇 6P 大电流转接板，板载 5.1kΩ CC pull-down
- [x] 研究 USB 信号分流芯片（如 USB Hub IC），实现板载一分二 → 不需要 Hub IC，被动 VBUS 注入即可
- [x] 对比方案成本：USB-C 母座 (~¥3) + Hub IC (~¥5) + 被动元件 vs 当前 ¥57.80 → ¥3.12/套 vs ¥57.80/套
- [x] 评估 JLCPCB SMT 贴片可行性（Hub IC 封装是否支持）→ 无需 SMT，DIP 转接板直插
- [x] **被动 SS34 注入实测**（2026-04-30）→ 失败，6P 转接板 + SS34 无法让 Tab A9 同时进入 Data Host + Power Sink，弃用为主线
- [x] **采购一分二 OTG + 充电线材**（2026-05-06）→ 已购买多种型号，测试出一种比较理想的可作为内置子板基准
- [x] **手工万能板原型集成**（2026-05-06，[图片](docs/assets/v4-handmade-prototype-2026-05-06.jpg)）→ ESP32-S3 DevKitC + 拆解后的一分二小板 + 5 路 XH2.54 连接器已焊出
- [x] 回归测试：HID 枚举 / 充电接入 / 旋钮 / 按钮 / NFC（PCB r1.0 上完成 2026-05-22）
- [ ] 整机长时间热稳定 + 充电链路 2h 测试（需要 Tab A9 到货后做）

### 按钮机构
> 选定方案：键盘轴 + 6.25U 卫星轴空格键，长度 ~10cm
> 状态：物料预计 04-02/03 到货，待手感测试
> ⚠️ **测试机变更（2026-05-06）**：原 R4 Pad（Arthur 寄来）因操作失误已损坏，临时使用安卓手机替代验证 USB HID + 充电链路。Tab A9 行为兼容性需在拿到新 Pad 后再回归一遍。

- [ ] 收到 Arthur 寄来的 Pad → 装配按钮测试手感
- [ ] Kailh BOX 轴 + 6.25U 卫星轴原型测试
- [ ] 3D 打印键帽适配（与 Arthur 外壳顶部模块配合）
- [ ] 行程验证 ≥ 3mm，稳定性测试
- [ ] **新 Tab A9 到货后**：回归 HID + 充电链路（当前在安卓手机上验证）

### NFC 模块
> 方案设计完成 ✅ 2026-04-01，详见 [docs/hardware/nfc.md](docs/hardware/nfc.md)
> 驱动库：`abobija/esp-idf-rc522` v3.4.3 (ESP Component Registry)
> 通信协议：HID 键盘逐字符键入 `NFC:<UID_HEX>\n`，平板端解析
> ✅ **NFC 数量已确认（2026-05-06）**：1 个 RC522 mini，最终方案，不再讨论。

- [x] 方案调研：模块选型、标签选型、驱动库评估、可行性分析 ✅ 2026-04-01
- [x] 确定安装位置：**外壳顶部模块内侧**（天线朝上） ✅ 2026-04-01
- [x] J4 连接器 pinout 确认（8P，与模块物理丝印一一对应） ✅ 2026-04-01
- [x] NFC 数量决策：1 个 ✅ 2026-05-06
- [x] RC522 mini SPI 驱动开发（ESP-IDF，abobija/rc522 v3.x） ✅ 2026-05-07
- [x] NTAG215 UID 读取 → HID 字符串键入实现 ✅ 2026-05-07
- [x] **NDEF Text Record 解析**：从卡上读出用户写入的字符串 ✅ 2026-05-07
- [x] 同卡 1.5s 去重（应对 RC522 心跳抖动） ✅ 2026-05-07
- [ ] 天线读取距离测试（穿透 3D 打印壁厚 2mm）— 留到外壳打印后

### 固件迁移
- [x] **设计 V4 引脚接线方案**（CLAUDE.md GPIO 表定稿，与万能板焊接一致） ✅ 2026-05-06
- [x] SuperMini → DevKitC N16R8 GPIO 重映射（input_handler.c / led_indicator.c） ✅ 2026-05-06
- [x] 新增 NFC SPI 模块（SPI2 GPIO Matrix，1MHz） ✅ 2026-05-07
- [x] 新增按钮 + 双 EC11 SW 处理 ✅ 2026-05-06
- [x] input_handler 扩展：NFC 事件 → `nfc_handler.c` 独立模块 + HID 字符串键入 ✅ 2026-05-07
- [x] 万能板上烧录验证：旋钮 / 按钮 / NFC 全链路 ✅ 2026-05-07
- [x] CLAUDE.md GPIO 分配表确认 ✅ 2026-05-06
- [x] `sdkconfig.defaults` 更新：N16R8 16MB flash + octal PSRAM ✅ 2026-05-08
- [x] HID Key Report 多键并发支持（`pressed_keys[6]` 状态数组 + mutex，消除 NFC 字符串注入与用户按键的竞态） ✅ 2026-05-08
- [x] NFC 启动失败容错：`nfc_handler_init/start` 失败时只 warning 不 abort，裸板（无 RC522）演示不再 reboot loop ✅ 2026-05-10
- [ ] NFC SPI 时钟从 1MHz 拉回 5MHz（需 PCB 焊好后验证，飞线版受 RF 噪声干扰）

### 旋钮与协议
> ✅ 协议定稿（2026-05-07，2026-05-15 简化）：详见 [README.md 交互设计](README.md#交互设计) 和 [CLAUDE.md HID Key Mappings](CLAUDE.md)
> NFC 主路径：固件读 NDEF Text → 原样键入 + Enter（无前缀，2026-05-15 起去掉 `#`）；UID 兜底用 `NFC:` 前缀

- [x] 编写旋钮 + NFC HID 协议（CLAUDE.md HID Key Mappings + NFC Tag 内容编码契约） ✅ 2026-05-07

### 外壳适配
> Arthur 已展示概念图，基本还原效果。设备可能两边各宽 1cm 增加安装灵活性。
> 充电口需留位，可水平插入。顶板螺丝固定，M2.5×10 平头内六角。
> **2026-05-08 起 Dehong 自己负责**：NFC 模块结构 + 长按条按钮模块结构 + 历史结构改进。

- [ ] **NFC 模块结构设计**（Dehong）：天线朝外、与 PCB 安装位对齐、走线通道
- [ ] **长按条按钮模块结构设计**（Dehong）：键盘轴 + 6.25U 卫星轴空格键，按压行程 + 装配方式
- [ ] **历史结构改进**（Dehong）：复盘 R1-R3 装配中暴露的问题点
- [ ] 与 Arthur 对接 Fusion 360 模型 (`cosmo-radio-v4-arthur`)
- [ ] 模块化验证：主体 / 顶部 / 背板 三件可独立打印
- [ ] PCB 安装孔位对齐（4 × M2.5）
- [ ] 线缆走线通道确认（取决于 USB 互联走线方案是否落地）
- [ ] Arthur 修改设计：充电口位置、设备宽度调整 → 打印小号模型验证
- [ ] NFC 位置确定后，协调另一侧用途（指示灯 vs 留空）

### 成本核算
> 单套估算 ¥79（不含线材）：主控板 ~¥32 (40%)，按钮+卫星轴 ~¥13，NFC IC ~¥13，底部 PCB 较便宜

- [x] 04-29 重启后合同价调整为 ¥5,000，原 12 套 ¥13,000 报价只作为历史参考
- [ ] 精确估算单套电路成本（待 PCB 设计定稿）
- [ ] 精确估算整机成本（含 3D 打印 + 组装工时）
- [ ] 重新估算单套原型的材料、PCB 打样、工时成本

### 组装与交付
- [ ] 1 套电子件：PCB 焊接 + 连接器 + 功能测试
- [ ] 1 套完整原型：打印 + 装配 + 整机测试
- [ ] 编写技术验证记录（PCB、USB-C、按钮、NFC、固件）
- [ ] 交付文件上传飞书

---

## 重启协议 2026-04-29（与 Arthur）

关键决策：
1. **范围**：仅做技术方案验证和单套原型制作
2. **报价**：¥5,000
3. **目标交付**：2026-05-20 左右
4. **核心挑战**：PCB 设计与验证
5. **合作定位**：保留和 Arthur 的持续硬件合作，不恢复原 4+8 批量交付合同

---

## 会议纪要 2026-04-01（与 Arthur/纪晟）

关键决策：
1. **音响**：使用平板自带音响，不外接
2. **按钮**：键盘轴 + 6.25U 卫星轴空格键（~10cm），物料预计 04-02/03 到
3. **NFC 数量**：待杨炜乐确认（1 vs 2），1 个 NFC = 8 根线，2 个 GPIO 开销过大
4. **螺丝**：M2.5×10 平头内六角
5. **旋钮按压**：EC11 SW 引脚会接线，功能可选不用
6. **设备宽度**：暂继续用弯头线，两边各宽 1cm 增加安装灵活性
7. **成本**：单套 ¥79（不含线材），12 套总计约 ¥13,000
8. **付款**：1 周后基础方案验证完成预付 50%

会后 Action Items：
- [ ] Arthur 与杨炜乐讨论 NFC 数量 → 结果告知 Dehong
- [ ] Arthur 寄 Pad → Dehong 收到后测试按钮手感
- [ ] Dehong 估算单套电路和整机价格
- [ ] Arthur 修改设计（充电口位置、宽度）→ 打印小号模型
- [ ] Dehong 编写旋钮 + NFC 协议规范文档

---

## 时间节点

| 日期 | 事项 |
|------|------|
| 04-01 | V4 正式启动，BOM 定稿，第一次需求会议 |
| 04-02 | 原 4 套整机 + 8 套电子套件合同取消 |
| 04-02/03 | 键盘轴物料到货，开始按钮手感测试 |
| 04-04 | Arthur 海运两个铁箱（现有设备） |
| 04-29 | 与 Arthur 重启 V4，范围缩小为技术验证 + 单套原型，合同价 ¥5,000 |
| 05-06 | 万能板飞线原型焊接，V4 GPIO 全部跑通 |
| 05-07 | NFC NDEF Text 解析 + `#<payload>\n` 协议落地，端到端跑通 |
| 05-08 | 固件 V4 彻底优化（sdkconfig + HID 多键并发）+ 结构件设计启动 + USB 互联走线验证 |
| 05-15 | NFC HID 协议简化：NDEF payload 去掉 `#` 前缀，固件 + 文档同步；端到端实卡验证通过（UID `04A38B6A220289`→`004`、`0463DA47220289`→`002`） |
| 05 中旬 | KiCad PCB 设计 + 外壳 3D 模型适配 |
| ~05-20 | 单套原型装配 + 交付技术验证结果 |
