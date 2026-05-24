# CosmoRadio V4 — 交付清单 / Handoff Manifest

> 用途：界定哪些产物**对客户交付**、哪些**仅内部**，并给出打包上传飞书的操作清单。
> 交付对象：杨炜乐（Putopia）/ Arthur（纪晟）
> 飞书交付文件夹：[CosmoRadio 项目](https://wcncnzoky497.feishu.cn/drive/folder/CQE8fA2R0lqKw1dIK1TchSWzndg)

---

## A. 对客户交付（上传飞书）

| # | 交付物 | 仓库路径 | 备注 |
|---|--------|----------|------|
| 1 | **技术验证报告** ⭐ | `docs/project/CosmoRadio-V4-技术验证报告.md` | 交付核心，含各子系统验证记录 + 已知限制 + r1.1 建议 |
| 2 | PCB 设计源（嘉立创EDA r1.0） | `pcb/cosmoradio-v4-jlceda/cosmoradio-v4-r1.0.epro2` | 可移植工程，客户可自行打开/打样 |
| 3 | PCB 原理图渲染 | `pcb/cosmoradio-v4-jlceda/cosmoradio-v4-r1.0-schematic.png` | |
| 4 | **PCB 制造文件** | `pcb/cosmoradio-v4-jlceda/manufacturing/` | ⏳ **待导出**（Gerber/BOM/坐标/PDF/3D，清单见 `pcb/README.md`）|
| 5 | PCB 生产/装配 spec | `docs/project/CosmoRadio-V4-PCB-Spec.md` | 连接器定义、RC 网络、QA 流程 |
| 6 | BOM 及报价分析 | `docs/project/CosmoRadio-V4-BOM及报价分析.md` | ⚠ 含历史报价，交付前可裁剪「报价记录」节 |
| 7 | 3D 模型全套 | `models/v4/` + `models/README.md` | 外壳 + 结构件，STL + Bambu 工程 |
| 8 | 硬件方案文档 | `docs/hardware/{nfc,usb,otg-adapter}.md` | NFC / USB / OTG 适配器设计 |
| 9 | 固件源码 | 仓库根 + `main/` | ESP-IDF 工程（如客户需要源码级交付）|
| 10 | HID 测试工具 | `test/hid-test.html` | 三圆布局网页测试器 |
| 11 | 工作原型（实物） | — | 1 套实焊验证 PCB + 装配原型，线下/寄送交付 |

## B. 仅内部（不上传客户）

| 产物 | 路径 | 原因 |
|------|------|------|
| 项目成本核算 | `docs/project/CosmoRadio-V4-项目成本核算.md` | 含人力/物料/token 成本，内部财务 |
| 项目主文档 | `docs/project/cosmoradio-yangweile-customized-hardware.md` | 含合同、财务、历史记录 |
| TODO | `TODO.md` | 内部进度 |
| CLAUDE.md | `CLAUDE.md` | 工程上下文 |
| OTG 充电调研 | `pcb/research/` | 内部技术调研 |

---

## C. 交付前待办

1. **导出 PCB 制造文件**（A-4）：用嘉立创EDA专业版打开 r1.0 工程，按 `pcb/README.md` 清单导出 Gerber/BOM/坐标/PDF/3D 到 `manufacturing/`。
2. **裁剪 BOM 报价节**（A-6）：交付版可移除「五、报价记录」里的内部毛利测算，只留物料 BOM。
3. **补原型实物照片**：拍 r1.0 实焊板 + 装配原型照片，加进验证报告/飞书（当前仓库只有飞线版照片）。
4. **post-delivery 回归**（需新 Tab A9）：Action Button 手感 + 整机热稳定/充电链路，结果回填验证报告 §5。

## D. 打包建议

飞书文件夹按 `01-验证报告 / 02-PCB / 03-模型 / 04-固件 / 05-文档` 分目录上传；A 表逐项对应。内部文档（B 表）不进客户文件夹。
