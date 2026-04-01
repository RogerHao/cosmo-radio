# CosmoRadio V4 — TODO

> 第四期：2026-04-01 → 2026-05-01
> 交付：4 套完整设备 + 8 套电子元件套件
> 项目文档：[docs/project/](docs/project/)（symlink → codex）

---

## 关键待办

### PCB 载板设计
- [ ] 确认 ESP32-S3 DevKitC N16R8 实际排针 pinout → 确定排母座间距
- [ ] KiCad 原理图：主控插座 + 4 组 XH2.54 连接器 + USB-C 母座
- [ ] PCB layout：80×50mm 双面板，参考 [PCB Spec](docs/project/CosmoRadio-V4-PCB-Spec.md)
- [ ] **评估 USB-C 板载集成方案**（见下方专项）
- [ ] JLCPCB 下单打样 (5 片)
- [ ] 焊接验证：排母座 + JST 插座 + USB-C 座

### USB-C 板载集成评估（成本优化重点）
> 当前方案：FPC 弯头 ¥24.80 + 一分二 ¥33.00 = **¥57.80/套 (占单套成本 43%)**
> 目标：评估能否将 Type-C 接口和充电/HID 分流功能集成到 PCB 上

- [ ] 研究 USB-C 母座直焊 PCB 方案（CC 引脚配置、ESD 保护）
- [ ] 研究 USB 信号分流芯片（如 USB Hub IC），实现板载一分二
- [ ] 对比方案成本：USB-C 母座 (~¥3) + Hub IC (~¥5) + 被动元件 vs 当前 ¥57.80
- [ ] 评估 JLCPCB SMT 贴片可行性（Hub IC 封装是否支持）
- [ ] 如可行，更新 PCB Spec 和原理图

### 按钮机构
- [ ] Kailh BOX 轴 + 6.25U 卫星轴原型测试
- [ ] 3D 打印键帽适配（与 Arthur 外壳顶部模块配合）
- [ ] 行程验证 ≥ 3mm，稳定性测试

### NFC 模块
- [ ] RC522 mini SPI 驱动开发（ESP-IDF）
- [ ] NTAG213 UID 读取 → HID 按键映射
- [ ] 确定安装位置：底板内侧 or 前面板背面
- [ ] 天线读取距离测试（穿透 3D 打印壁厚 2mm）

### 固件迁移
- [ ] SuperMini → DevKitC N16R8 GPIO 重映射
- [ ] 新增 NFC SPI 模块 (FSPI: GPIO34-37)
- [ ] 新增按钮模块（Kailh 轴 GPIO7）
- [ ] input_handler 扩展：NFC 事件 → HID 按键
- [ ] 更新 CLAUDE.md 中的 GPIO 分配表

### 外壳适配
- [ ] 与 Arthur 对接 Fusion 360 模型 (`cosmo-radio-v4-arthur`)
- [ ] 模块化验证：主体 / 顶部 / 背板 三件可独立打印
- [ ] PCB 安装孔位对齐（4 × M2.5）
- [ ] 线缆走线通道确认

### 组装与交付
- [ ] 12 套电子件：PCB 焊接 + 连接器 + 功能测试
- [ ] 4 套完整设备：打印 + 装配 + 整机测试
- [ ] 编写图文组装指南（电子套件客户自行组装用）
- [ ] 交付文件上传飞书

---

## 时间节点

| 日期 | 事项 |
|------|------|
| 04-01 | R4 正式启动，BOM 定稿 |
| 04-04 | Arthur 海运两个铁箱（现有设备） |
| 04 第一周 | 测试套件到货，按钮原型验证 |
| 04 第二周 | PCB 设计完成，JLCPCB 下单；NFC 驱动开发 |
| 04 第三周 | PCB 到货焊接验证；固件集成测试 |
| 04 第四周 | 批量组装 + 整机测试 |
| 05-01 | 交付 |
