# CosmoRadio 3D 模型

外壳与结构件的 3D 打印模型。**`v4/` 是 V4 技术验证交付的最终模型集**，对应实焊验证通过的单套原型（iMac G3 斜面站立式，三圆形"IOI"遮罩 + 侧面 Action Button）。Fusion 360 装配工程名 `cosmo-radio-v4-prototype`。

> ⚠️ **模型二进制文件（STL/3MF/STEP）被 `.gitignore` 排除，不进 git**（大文件资产策略）。本 README 是 git 里唯一的模型清单。**交接/交付时，实际模型文件走飞书或单独拷贝**，不能靠 clone 仓库获得。Fusion 360 源工程在 Autodesk 云端（`cosmo-radio-v4-prototype`）。

> 文件约定：`.3mf` = Bambu Studio 工程/切片文件（含打印参数）；`.stl` = 网格导出（通用）。
> 打印机：拓竹 P2S。

## v4/ — 最终交付模型集

装配顶层部件（以 Fusion 装配为准）：

| 部件 | 文件 | 说明 |
|------|------|------|
| 主体壳 | `cosmo-radio-v4-prototype-main.{stl,3mf}` | iMac G3 斜面主体，三圆遮罩开孔 |
| 背板 | `cosmo-radio-v4-prototype-back.{stl,3mf}` / `…-back-with-hole.stl` | 可替换背板（含开孔变体） |
| 按钮座 | `button-base.{stl,3mf}` | Action Button 安装座（键盘轴 + 6.25U 卫星轴） |
| 按钮 | `button.{stl,3mf}` / `button-round.stl` / `button-spring.{stl,3mf}` | 键帽 + 弹性结构 |
| NFC 座 | `nfc-base.{stl,3mf}` / `nfc-base-print.{stl,3mf}` | RC522 模块安装座（天线朝外） |
| 旋钮帽 | `rotary-knob.{stl,3mf}` | EC11 旋钮帽 ×2 |
| 脚垫 | `foot.{stl,3mf}` | 底部支脚 |
| 充电口背件 | `back-charger.{stl,3mf}` | 充电口位结构 |
| 内部紧固件 | `pad-faster.{stl,3mf}` / `nfc-faster.{stl,3mf}` | 平板/NFC 固定件（`*-faster` = 紧固件，非夹具） |
| NFC 卡座 | `nfc-tag.3mf` | NFC 标签相关 |

> `Cube.3mf` 为切片软件默认占位对象，非交付件。

## 历史版本（仅参考）

| 目录 | 版本 | 主控 | 形态 |
|------|------|------|------|
| `v1/` | V1（2025 Q3-Q4） | Adafruit HUZZAH32 | 硬件预研，多版前后壳迭代 |
| `v2/` | V2（2025.12-2026.01） | XIAO ESP32S3 | BLE HID，硅谷首批组装 |
| `CosmoRadio-V3-模型及拓竹P2S打印工程切片文件/` | V3（2026.01-2026.02） | ESP32-S3 SuperMini | USB HID，12 套交付（含 STEP 源 + P2S 切片） |

V3 目录含完整 `.step` 参数化源文件；v1/v2/v4 主要为 STL/3MF。
