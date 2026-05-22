#!/usr/bin/env python3
"""
Post-process the routed CosmoRadio V4 carrier PCB.

This preserves Freerouting copper while applying deterministic cleanup:
- move the board so Edge.Cuts starts at (0, 0)
- widen only the short 5V and USB data tracks that can pass DRC
- replace generated connector silk with clearer labels and pin-order hints
"""

from pathlib import Path

import pcbnew

PROJECT = Path(__file__).resolve().parent.parent
PROJECT_NAME = "CosmoradioV4Carrier"
PCB_PATH = PROJECT / PROJECT_NAME / f"{PROJECT_NAME}.kicad_pcb"

OLD_SILK_TEXT = {
    "CosmoRadio V4 r1.0",
    "dehonghao.com",
    "OTG USB-C",
    "DO NOT PLUG",
    "(use UART)",
    "J5 USB",
    "J1 EC11-L",
    "J2 EC11-R",
    "J3 BTN",
    "J4 NFC",
    "5V D- D+ G",
    "SW G A G B",
    "SIG G",
    "CS SCK MOSI MISO IRQ G RST 3V3",
}


def move_board_to_zero(board: pcbnew.BOARD) -> None:
    xs: list[int] = []
    ys: list[int] = []
    for item in board.GetDrawings():
        if item.GetLayer() != pcbnew.Edge_Cuts or not hasattr(item, "GetStart"):
            continue
        xs.extend([item.GetStart().x, item.GetEnd().x])
        ys.extend([item.GetStart().y, item.GetEnd().y])

    if xs and ys:
        board.Move(pcbnew.VECTOR2I(-min(xs), -min(ys)))


def tune_track_widths(board: pcbnew.BOARD) -> None:
    mm = pcbnew.FromMM
    for track in board.GetTracks():
        if track.Type() == pcbnew.PCB_VIA_T:
            continue
        net = track.GetNetname()
        if net == "5V":
            track.SetWidth(mm(0.2))
        elif net in {"USB_DM", "USB_DP"}:
            track.SetWidth(mm(0.25))


def replace_generated_silk(board: pcbnew.BOARD) -> None:
    for item in list(board.GetDrawings()):
        if (
            item.Type() == pcbnew.PCB_TEXT_T
            and item.GetLayer() == pcbnew.F_SilkS
            and item.GetText() in OLD_SILK_TEXT
        ):
            board.Remove(item)

    mm = pcbnew.FromMM

    def add_silk(text: str, x: float, y: float, size_mm: float = 1.0) -> None:
        item = pcbnew.PCB_TEXT(board)
        item.SetText(text)
        item.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        item.SetLayer(pcbnew.F_SilkS)
        item.SetTextSize(pcbnew.VECTOR2I(mm(size_mm), mm(size_mm)))
        item.SetTextThickness(mm(0.15))
        board.Add(item)

    add_silk("CosmoRadio V4 r1.0", 37.5, 96.0, 1.2)
    add_silk("dehonghao.com", 42.0, 98.5, 1.0)
    add_silk("OTG USB-C", 10.0, 17.0, 1.0)
    add_silk("DO NOT PLUG", 10.0, 20.0, 1.0)
    add_silk("USE UART", 11.0, 23.0, 0.9)

    add_silk("J5 USB", 44.0, 3.0, 0.9)
    add_silk("5V D- D+ G", 42.0, 8.5, 0.8)
    add_silk("J1 EC11-L", 10.0, 40.0, 0.9)
    add_silk("SW G A G B", 14.0, 68.0, 0.8)
    add_silk("J2 EC11-R", 73.0, 40.0, 0.9)
    add_silk("SW G A G B", 76.0, 68.0, 0.8)
    add_silk("J3 BTN", 13.0, 86.0, 0.9)
    add_silk("SIG G", 22.0, 88.5, 0.8)
    add_silk("J4 NFC", 55.0, 86.0, 0.9)
    add_silk("CS SCK MOSI MISO IRQ G RST 3V3", 41.0, 88.5, 0.8)


def main() -> int:
    board = pcbnew.LoadBoard(str(PCB_PATH))
    move_board_to_zero(board)
    tune_track_widths(board)
    replace_generated_silk(board)
    board.Save(str(PCB_PATH))
    print(f"saved {PCB_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
