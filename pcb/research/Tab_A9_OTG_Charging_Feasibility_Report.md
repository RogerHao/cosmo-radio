# CosmoRadio V4 专用 Tab A9 半主动 USB-C OTG + 充电方案可行性调研报告

## 1. 方案工作原理与机制分析

在 USB-C 和 Android 体系下，实现平板作为 USB Host（控制 ESP32-S3）同时接受外部充电，涉及到 USB 角色协商和电源路径管理的复杂机制。

### 1.1 标准行为与设备特定行为
*   **标准 USB-C DRP (Dual Role Port) 行为**：标准的 USB-C 接口通过 CC (Configuration Channel) 引脚进行角色协商。供电端（Source）在 CC 引脚上提供上拉电阻（Rp），受电端（Sink）提供下拉电阻（Rd，5.1kΩ）。双角色端口（DRP）在未连接时会交替呈现 Rp 和 Rd。当平板作为 USB Host 时，标准行为是它必须作为供电端（Source），向外输出 5V VBUS，此时其内部 CC 逻辑呈现 Rp [1]。
*   **SimulCharge / ACA (Accessory Charger Adapter) 机制**：在早期的 Micro-USB 时代，USB BC 1.2 规范定义了 ACA 机制，通过在 ID 引脚上接特定阻值的电阻（如 124kΩ 或 36.5kΩ），允许设备同时作为 USB Host 并接受充电 [2]。
*   **设备特定行为（Samsung 专有实现）**：在 USB-C 时代，虽然 USB PD 规范支持 Power Role Swap（电源角色交换）和 Data Role Swap（数据角色交换），但这需要完整的 PD 协议栈支持。对于低成本的半主动方案（不使用 PD 芯片），三星等厂商在底层固件中保留了类似 ACA 的模拟检测逻辑，或者允许在特定的 CC 电阻组合及 VBUS 强制注入时，触发“Host 模式下充电”的隐藏状态。这属于**高度设备特定**的行为，严重依赖于平板的内核驱动和充电 IC 固件 [3]。

## 2. 可行电路拓扑建议

针对“低复杂度、可集成到 PCB、不使用完整 PD 固件栈”的需求，建议采用以下半主动注入拓扑：

### 2.1 核心连接关系
1.  **外部充电器 VBUS 注入**：外部 5V 电源通过一个低压降的肖特基二极管（如 SS14 或 SS24）或者 P-MOSFET 理想二极管电路，注入到连接平板的 VBUS 线上。
2.  **平板端 CC 引脚配置**：
    *   在连接平板的 USB-C 插头的 CC 引脚上，需要配置特定的电阻网络来“欺骗”平板。
    *   **常规尝试**：接 5.1kΩ 下拉电阻（Rd）到地，使平板认为连接了充电器（进入 Sink 模式接受充电），同时强行在 D+/D- 上进行数据通信。
    *   **三星特定尝试（模拟 ACA）**：部分三星设备可能需要特定的 Rp 上拉（例如 56kΩ 到 5V）或者在 CC 线上施加特定电压，配合 D+/D- 上的设备连接，才能触发 SimulCharge 模式。
3.  **ESP32-S3 端**：ESP32 的 USB D+/D- 直接连接到平板的 D+/D-。ESP32 的供电可以直接取自外部注入的 5V VBUS。
4.  **防反灌保护**：**必须**在平板 VBUS 和外部充电器 VBUS 之间加入隔离。如果不加隔离，当外部电源断开时，平板会试图向外输出 5V（Host 模式默认行为），导致平板电池被外部电路迅速抽干 [4]。

### 2.2 推荐验证拓扑（第一版）
*   **VBUS 路径**：外部 5V $\rightarrow$ 肖特基二极管（阳极） $\rightarrow$ 肖特基二极管（阴极） $\rightarrow$ 平板 VBUS。同时，外部 5V 直接供电给 ESP32。
*   **CC 引脚**：平板侧 CC1/CC2 尝试下拉 5.1kΩ 到 GND（基础 Sink 测试），或预留焊盘以便测试悬空/上拉。
*   **数据线**：ESP32 D+/D- 直连平板 D+/D-（注意 USB 2.0 走线尽量等长）。

## 3. 最小样板验证步骤

在进行 PCB 投板前，必须使用洞洞板或简单的转接板进行验证。

### 3.1 验证准备
*   准备一个 USB-C 母座（接外部电源）、一个 USB-C 公头（接 Tab A9）、一个 ESP32-S3 DevKit。
*   准备万用表、示波器、低压降肖特基二极管（如 SS24）、5.1kΩ 电阻。

### 3.2 测试步骤与测量项
1.  **基础充电测试**：只连接外部 5V 和 CC 下拉电阻到平板，万用表测量平板 VBUS 电压，确认平板是否显示充电。
2.  **数据连通测试**：在步骤 1 基础上，将 ESP32 的 D+/D- 接入，观察 ESP32 是否成功枚举为 HID 设备。
3.  **VBUS 冲突测试（关键）**：断开外部 5V，万用表测量连接平板侧的 VBUS，看平板是否向外输出 5V。测量二极管阳极，确认没有电压反灌。
4.  **枚举中断测试**：在 ESP32 正常工作时，插拔外部 5V 电源，观察 ESP32 的 HID 连接是否会断开重连。

### 3.3 通过/失败标准
*   **通过**：Tab A9 显示充电（即使是慢充），且 ESP32 稳定枚举为 HID 并能发送数据；插拔充电器不导致 ESP32 掉线或仅有短暂可恢复的掉线。
*   **失败**：Tab A9 拒绝充电；或者充电时无法识别 ESP32；或者外部电源断开时，平板反向供电导致系统不稳定。

## 4. Tab A9 兼容性实测证据

针对 Samsung Galaxy Tab A9 (SM-X110) 的 SimulCharge 兼容性，调研结果**非常不乐观**。

1.  **LAVA 官方兼容性列表**：LAVA 是提供 SimulCharge 适配器的权威厂商。在其 Charge-Plus 兼容性列表中，**未列出**基础版 Tab A9 (SM-X110)。列表中仅提到了 Tab A7 Lite、Tab A8 等，且明确指出部分型号（如 Tab A7 Lite SM-T220）存在严重的电流限制 bug（电量不同时拉取的电流不同，甚至低至 0.5A），并建议**不要使用** [5]。
2.  **用户社区反馈**：在三星社区和 Reddit 上，多位用户反馈 Tab A9 和 A9+ 无法在使用普通 USB-C Hub 时同时进行 OTG 和充电。有用户明确表示“连接 USB-C 输入会点亮适配器，Android 显示正在充电，但无法与外设工作” [6]。
3.  **底层限制原因**：LAVA 的技术博客指出，如果移动设备没有实现完整的 USB-C PD 规范（Power Delivery），就无法与适配器协商电源契约，从而无法实现同时供电和数据传输 [7]。Tab A9 作为入门级平板，其 USB-C 接口仅支持 USB 2.0 且大概率**阉割了完整的 PD 角色交换功能**。

## 5. 可采购器件建议 (LCSC/立创商城)

为了控制 BOM 复杂度，建议采购以下基础器件进行验证：

*   **肖特基二极管（防反灌）**：
    *   **SS24** (Slkor / 长电)：2A，40V。价格极低（约 0.05 RMB），适合基础验证，但压降较大（约 0.5V）。
    *   **SS34**：3A 版本，余量更大。
*   **P-MOSFET（替代肖特基，降低压降）**：
    *   **AO3401A** (AOS / 长电)：-4.2A，-30V，SOT-23 封装。常用于简单的 VBUS 开关和防反接电路，导通电阻低 [8]。
*   **ESD 保护**：
    *   **USBLC6-2SC6** (ST / 各种国产替代)：专为 USB 2.0 D+/D- 设计的超低电容 ESD 阵列，SOT23-6 封装。

## 6. 风险清单

*   **平板拒绝充电 / 充电电流不足**：由于缺乏 PD 协商，平板可能只以默认的 500mA 甚至拒绝充电。
*   **枚举中断**：在插拔外部充电器时，VBUS 电压的波动可能导致 ESP32 重新启动或 USB 重新枚举，导致操作中断。
*   **反灌风险**：如果防反灌二极管失效或 P-MOSFET 栅极控制逻辑错误，外部 5V 和平板输出的 5V 发生冲突，可能烧毁平板接口。
*   **CC 配置错误**：错误的电阻值可能导致平板进入错误的模式（如纯供电模式），完全无法识别设备。

## 7. 最终结论与建议

**结论：针对 Tab A9 (SM-X110) 做纯硬件的“半主动”验证小板，风险极高，成功率较低，但考虑到项目成本限制，值得进行一次低成本的“死马当活马医”式验证。**

原因在于，大量证据表明 Tab A9 缺乏完整的 PD 协议支持，三星很可能在固件层面屏蔽了简单的电阻欺骗法。如果基础的下拉/上拉验证失败，则说明该平板**必须**依赖带 PD 芯片的复杂方案（如通过 FUSB302 等芯片进行角色交换协商），这将违背“低复杂度”的初衷。

### 第一版验证板原理图级连接清单：

1.  **VBUS_IN (外部 5V)** 连接到 **Diode_Anode** (肖特基阳极)。
2.  **Diode_Cathode** (肖特基阴极) 连接到 **Tab_A9_VBUS**。
3.  **VBUS_IN** 连接到 **ESP32_5V_IN** (为 ESP32 供电)。
4.  **Tab_A9_CC1** 和 **Tab_A9_CC2** 各接一个 **5.1kΩ** 电阻到 **GND**（预留断开跳线）。
5.  **Tab_A9_D+** 直连 **ESP32_D+**。
6.  **Tab_A9_D-** 直连 **ESP32_D-**。
7.  所有设备的 **GND** 共地。

*建议先用切断 VBUS 线的 USB 数据线配合面包板进行测试，若步骤 3.2 中的测试均失败，则应果断放弃半主动方案，考虑更换平板型号或接受无法边充边用的现实。*

---
### 参考文献
[1] Texas Instruments. A Primer on USB Type-C and Power Delivery Applications.
[2] StackExchange. Can an Android tablet serve as USB Host and be charged simultaneously.
[3] XDA Forums. Guide..OTG USB HOST and CHARGING at same time for Galaxy tabs.
[4] Reddit. USB-C Prevent back powering in Host OTG-Mode.
[5] LAVA Accessory. Charge-Plus Compatibility List.
[6] Samsung Community. Galaxy Tab A9 USB and charging at the same time.
[7] LAVA Blog. Why Some USB-C Mobile Devices Can't Do Simultaneous Power and Data.
[8] LCSC. AO3401A Datasheet and Pricing.
