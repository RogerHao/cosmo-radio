"""
build_pcb.py — CosmoRadio V4 carrier PCB layout (Plan v2, 100×100mm)

Run with KiCad's bundled Python:
    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \\
        scripts/build_pcb.py

Layout (USB-C end at top = PCB y=0 = back of enclosure):
  - U1A/U1B: 两条 1×22 单排母 居中，间距 22.86mm
  - J5 USB 4P: 顶部中央，靠近 USB-C 端
  - J4 NFC 8P: 右侧中上 (GPIO 4-7, 15-16 在 U1B 中段)
  - J2 EC11-R 5P: 右侧中段 (GPIO 17/18/8)
  - J1 EC11-L 5P: 左侧中段 (GPIO 42/41/40)
  - J3 BTN 2P: 左侧靠底 (GPIO 1)
  - R1-R4 + C1-C4: 紧贴 J1/J2 的 A/B pin
  - H1-H4: 四角 M2.5
"""

import pcbnew
import re
import sys
from pathlib import Path

# ============================================================
# PROJECT CONFIG
# ============================================================

PROJECT      = Path(__file__).resolve().parent.parent
PROJECT_NAME = "CosmoradioV4Carrier"
PCB_PATH = PROJECT / f"{PROJECT_NAME}/{PROJECT_NAME}.kicad_pcb"
NET_PATH = PROJECT / f"{PROJECT_NAME}/{PROJECT_NAME}.net"

KICAD_FP_BASE = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")
KICAD_3D_BASE = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels")

BOARD_W_MM = 100.0
BOARD_H_MM = 100.0

# ============================================================
# PLACEMENT
# ============================================================
# Coordinate system: (0,0) top-left, +x right, +y down
# U1A 和 U1B 中心距 = 22.86mm (DevKitC 排针标准)
# Socket footprint 锚点 = pin 1 位置，不是中心。
# 1×22 socket 总长 = 21 × 2.54mm = 53.34mm。
# 要让 socket 在板 Y 方向居中（pin 1 → pin 22 横跨 y=23.33 → y=76.67），
# 须将 socket 的 y 锚点设为 23.33。
U1_Y_PIN1 = 23.33           # pin 1 (顶部, 靠近 USB-C 端)
U1A_X = (BOARD_W_MM / 2) - (22.86 / 2)   # = 38.57
U1B_X = (BOARD_W_MM / 2) + (22.86 / 2)   # = 61.43

PLACEMENT = {
    # ---- DevKitC sockets (居中, 两条 1×22 间距 22.86mm) ----
    # rot=0: 默认 footprint pin row 已经是竖直（沿 Y 轴）
    # y 锚点 = pin 1 位置（顶部）
    "U1A1": dict(lib="Connector_PinSocket_2.54mm", fp="PinSocket_1x22_P2.54mm_Vertical",
                 x=U1A_X, y=U1_Y_PIN1, rot=0, value="DevKitC-L"),
    "U1B1": dict(lib="Connector_PinSocket_2.54mm", fp="PinSocket_1x22_P2.54mm_Vertical",
                 x=U1B_X, y=U1_Y_PIN1, rot=0, value="DevKitC-R"),

    # ---- B-type Vertical (top-entry, 立式直插) JST footprint ----
    # cable 从上往下 (Z+) 垂直插入; 旋转控制 latch (豁口) 朝向哪条板边
    # B-type 实测 latch 方向 (从 silk U 形开口反推):
    #   rot=0   → latch 朝 UP (-Y)
    #   rot=90  → latch 朝 RIGHT (+X)
    #   rot=180 → latch 朝 DOWN (+Y)
    #   rot=270 → latch 朝 LEFT (-X)

    # ---- J5 USB 顶部中央, latch 朝 UP (朝外壳后部 USB-C 出线方向) ----
    "J5":  dict(lib="Connector_JST", fp="JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical",
                x=46.25, y=12.0, rot=0, value="USB-DONGLE"),
    # rot=0: pin row 水平, pin 1 at x=46.25, pin 4 at x=53.75. 中心 x=50.

    # ---- J4 NFC 右下, latch 朝 DOWN ----
    "J4":  dict(lib="Connector_JST", fp="JST_XH_B8B-XH-A_1x08_P2.50mm_Vertical",
                x=73.75, y=92.0, rot=180, value="NFC"),
    # rot=180: pin row 水平(-X), pin 1 at x=73.75, pin 8 at x=56.25. 中心 x=65.

    # ---- J1 EC11-L 左中, latch 朝外侧 ----
    # rot=90: EC11 母座相对上一版旋转 180°, pin 1 at (8, 55), pin 5 at (8, 45)
    "J1":  dict(lib="Connector_JST", fp="JST_XH_B5B-XH-A_1x05_P2.50mm_Vertical",
                x=8.0, y=55.0, rot=90, value="EC11-L"),

    # ---- J2 EC11-R 右中, latch 朝外侧 ----
    # rot=270: EC11 母座相对上一版旋转 180°, pin 1 at (92, 45), pin 5 at (92, 55)
    "J2":  dict(lib="Connector_JST", fp="JST_XH_B5B-XH-A_1x05_P2.50mm_Vertical",
                x=92.0, y=45.0, rot=270, value="EC11-R"),

    # ---- J3 BTN 左下, latch 朝 DOWN ----
    "J3":  dict(lib="Connector_JST", fp="JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical",
                x=16.25, y=92.0, rot=180, value="BTN"),
    # rot=180: pin 1 at x=16.25, pin 2 at x=13.75. 中心 x=15.

    # ---- R/C for J1 (ENC1 A/B), 紧贴 J1 (左侧) 与 U1A (中段) 之间 ----
    # ENC1_A net = J1.3(y=50) + U1A.17(y=63.97) → R1/C1 在 y=50 附近, x 在 J1 和 U1A 之间
    # ENC1_B net = J1.5(y=45) + U1A.16(y=61.43) → R2/C2 在 y=45 附近
    "R1":  dict(lib="Resistor_SMD", fp="R_0805_2012Metric",
                x=20.0, y=50.0, rot=0,   value="10k"),    # ENC1_A pull-up
    "C1":  dict(lib="Capacitor_SMD", fp="C_0805_2012Metric",
                x=24.0, y=50.0, rot=0,   value="10nF"),   # ENC1_A debounce
    "R2":  dict(lib="Resistor_SMD", fp="R_0805_2012Metric",
                x=20.0, y=45.0, rot=0,   value="10k"),    # ENC1_B pull-up
    "C2":  dict(lib="Capacitor_SMD", fp="C_0805_2012Metric",
                x=24.0, y=45.0, rot=0,   value="10nF"),   # ENC1_B debounce

    # ---- R/C for J2 (ENC2 A/B), 紧贴 U1B (中段) 与 J2 (右侧) 之间 ----
    # ENC2_A net = J2.3(y=50) + U1B.13(y=53.81)
    # ENC2_B net = J2.5(y=55) + U1B.12(y=51.27)
    "R3":  dict(lib="Resistor_SMD", fp="R_0805_2012Metric",
                x=80.0, y=50.0, rot=0,   value="10k"),    # ENC2_A pull-up
    "C3":  dict(lib="Capacitor_SMD", fp="C_0805_2012Metric",
                x=76.0, y=50.0, rot=0,   value="10nF"),   # ENC2_A debounce
    "R4":  dict(lib="Resistor_SMD", fp="R_0805_2012Metric",
                x=80.0, y=55.0, rot=0,   value="10k"),    # ENC2_B pull-up
    "C4":  dict(lib="Capacitor_SMD", fp="C_0805_2012Metric",
                x=76.0, y=55.0, rot=0,   value="10nF"),   # ENC2_B debounce

    # ---- 4 个 M2.5 安装孔 (四角, 5mm 内偏) ----
    "H1":  dict(lib="MountingHole", fp="MountingHole_2.7mm_M2.5",
                x=5.0, y=5.0, rot=0, value=""),    # 左上
    "H2":  dict(lib="MountingHole", fp="MountingHole_2.7mm_M2.5",
                x=95.0, y=5.0, rot=0, value=""),   # 右上
    "H3":  dict(lib="MountingHole", fp="MountingHole_2.7mm_M2.5",
                x=95.0, y=95.0, rot=0, value=""),  # 右下
    "H4":  dict(lib="MountingHole", fp="MountingHole_2.7mm_M2.5",
                x=5.0, y=95.0, rot=0, value=""),   # 左下
}


# ============================================================
# Helpers
# ============================================================

def parse_netlist(path: Path):
    text = path.read_text()
    pad_nets = {}
    for nm in re.finditer(r'\(net\s+\(code\s+"\d+"\)\s+\(name\s+"([^"]+)"\)', text):
        net_name = nm.group(1).lstrip("/")
        if net_name.startswith("unconnected-"):
            continue
        pos, depth, end = nm.start(), 0, None
        while pos < len(text):
            if text[pos] == "(": depth += 1
            elif text[pos] == ")":
                depth -= 1
                if depth == 0: end = pos; break
            pos += 1
        body = text[nm.end():end]
        for n in re.finditer(r'\(node\s+\(ref\s+"([^"]+)"\)\s+\(pin\s+"([^"]+)"\)', body):
            pad_nets[(n.group(1), n.group(2))] = net_name
    return pad_nets


def patch_3d_paths(footprint: pcbnew.FOOTPRINT) -> None:
    """Make every 3D model path absolute (no ${KICAD*_3DMODEL_DIR} env var)."""
    abs_3d = str(KICAD_3D_BASE)
    for model in footprint.Models():
        new = model.m_Filename
        for var in ("${KICAD6_3DMODEL_DIR}", "${KICAD7_3DMODEL_DIR}",
                    "${KICAD8_3DMODEL_DIR}", "${KICAD9_3DMODEL_DIR}",
                    "${KICAD10_3DMODEL_DIR}"):
            new = new.replace(var, abs_3d)
        if new.endswith(".wrl"):
            step_candidate = new[:-4] + ".step"
            if Path(step_candidate).exists():
                new = step_candidate
        model.m_Filename = new


def main() -> int:
    pad_nets = parse_netlist(NET_PATH)
    print(f"[netlist] {len(pad_nets)} pad-net mappings")

    all_nets = sorted(set(pad_nets.values()))
    board = pcbnew.NewBoard(str(PCB_PATH))

    # Register nets
    net_objs = {}
    for n in all_nets:
        ni = pcbnew.NETINFO_ITEM(board, n)
        board.Add(ni)
        net_objs[n] = ni

    # Place footprints
    io = pcbnew.PCB_IO_KICAD_SEXPR()
    mm = pcbnew.FromMM
    for ref, p in PLACEMENT.items():
        lib_path = str(KICAD_FP_BASE / f"{p['lib']}.pretty")
        fp = io.FootprintLoad(lib_path, p["fp"])
        if not fp:
            print(f"[fail] could not load {p['lib']}:{p['fp']}")
            return 1
        fp.SetReference(ref)
        fp.SetValue(p.get("value", ""))
        fp.SetPosition(pcbnew.VECTOR2I(mm(p["x"]), mm(p["y"])))
        fp.SetOrientationDegrees(p["rot"])

        for pad in fp.Pads():
            pin = pad.GetNumber()
            net_name = pad_nets.get((ref, pin))
            if net_name and net_name in net_objs:
                pad.SetNet(net_objs[net_name])

        patch_3d_paths(fp)
        board.Add(fp)
        print(f"[place] {ref:6s} ({p['lib']}:{p['fp']}) at ({p['x']:5.1f},{p['y']:5.1f}) rot={p['rot']}")

    # Edge.Cuts rectangle 100×100mm
    for x1, y1, x2, y2 in [(0, 0, BOARD_W_MM, 0),
                           (BOARD_W_MM, 0, BOARD_W_MM, BOARD_H_MM),
                           (BOARD_W_MM, BOARD_H_MM, 0, BOARD_H_MM),
                           (0, BOARD_H_MM, 0, 0)]:
        line = pcbnew.PCB_SHAPE(board)
        line.SetShape(pcbnew.SHAPE_T_SEGMENT)
        line.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        line.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        line.SetLayer(pcbnew.Edge_Cuts)
        line.SetWidth(mm(0.1))
        board.Add(line)
    print(f"[edge] {BOARD_W_MM:.0f} × {BOARD_H_MM:.0f} mm rectangle")

    # Silkscreen labels (min 1.0mm height per JLC DFM)
    def add_silk(text: str, x: float, y: float, size_mm: float = 1.0, rot: float = 0):
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size_mm), mm(size_mm)))
        t.SetTextThickness(mm(0.15))
        t.SetTextAngleDegrees(rot)
        board.Add(t)

    # ---- 板级标题 ----
    add_silk("CosmoRadio V4 r1.0", BOARD_W_MM / 2 - 12, BOARD_H_MM - 4, 1.2)
    add_silk("dehonghao.com", BOARD_W_MM / 2 - 8, BOARD_H_MM - 1.5, 1.0)

    # ---- 警示丝印 (放在 U1A/U1B 左上空白区) ----
    add_silk("OTG USB-C",   14.0, 17.0, 1.0)
    add_silk("DO NOT PLUG", 14.0, 20.0, 1.0)
    add_silk("(use UART)",  16.0, 23.0, 0.9)

    # ---- 连接器标签 + pin 顺序 ----
    add_silk("J5 USB",    44.0, 3.0,  0.9)          # J5 上方边缘
    add_silk("5V D- D+ G", 42.0, 8.5, 0.8)
    add_silk("J1 EC11-L", 10.0, 40.0, 0.9)          # J1 上方
    add_silk("SW G A G B", 14.0, 68.0, 0.8)
    add_silk("J2 EC11-R", 73.0, 40.0, 0.9)          # J2 上方
    add_silk("SW G A G B", 76.0, 68.0, 0.8)
    add_silk("J3 BTN",    13.0, 86.0, 0.9)          # J3 上方
    add_silk("SIG G",      22.0, 88.5, 0.8)
    add_silk("J4 NFC",    55.0, 86.0, 0.9)          # J4 上方
    add_silk("CS SCK MOSI MISO IRQ G RST 3V3", 41.0, 88.5, 0.8)

    board.Save(str(PCB_PATH))
    print(f"\n✅ saved {PCB_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
