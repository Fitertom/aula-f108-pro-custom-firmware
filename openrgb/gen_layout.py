"""Генерация AULAF108ProLayout.h из patch/keymap.json (карта клавиш приложения AULA + найденные зоны).
python gen_layout.py
"""
import json
from pathlib import Path

root = Path(__file__).parent
km = json.loads((root / "keymap.json").read_text(encoding="utf-8"))

# HID usage (из xml) -> имя клавиши в стиле OpenRGB
NAMES = {0x29: "Escape", 0x28: "Enter", 0x2a: "Backspace", 0x2b: "Tab", 0x2c: "Space", 0x35: "`", 0x2d: "-", 0x2e: "=",
         0x2f: "[", 0x30: "]", 0x31: "\\\\", 0x33: ";", 0x34: "'", 0x36: ",", 0x37: ".", 0x38: "/", 0x39: "Caps Lock",
         0x46: "Print Screen", 0x47: "Scroll Lock", 0x48: "Pause/Break", 0x49: "Insert", 0x4a: "Home", 0x4b: "Page Up",
         0x4c: "Delete", 0x4d: "End", 0x4e: "Page Down", 0x4f: "Right Arrow", 0x50: "Left Arrow", 0x51: "Down Arrow",
         0x52: "Up Arrow", 0x53: "Num Lock", 0x54: "Number Pad /", 0x55: "Number Pad *", 0x56: "Number Pad -",
         0x57: "Number Pad +", 0x58: "Number Pad Enter", 0x62: "Number Pad 0", 0x63: "Number Pad .", 0x65: "Menu",
         0xe0: "Left Control", 0xe1: "Left Shift", 0xe2: "Left Alt", 0xe3: "Left Windows", 0xe4: "Right Control",
         0xe5: "Right Shift", 0xe6: "Right Alt", 0xaf: "Right Fn"}
for i in range(26):
    NAMES[0x04 + i] = chr(ord("A") + i)
for i in range(9):
    NAMES[0x1e + i] = str(i + 1)
NAMES[0x27] = "0"
for i in range(12):
    NAMES[0x3a + i] = f"F{i + 1}"
for i in range(9):
    NAMES[0x59 + i] = f"Number Pad {i + 1}"

keys = []
seen = set()
for k in km["keys"]:
    code = int(k["code"], 16)
    name = NAMES.get(code)
    if name is None:
        raise SystemExit(f"нет имени для кода {k['code']} ({k['name']})")
    if name in seen:           # дубль имени (напр. два Enter на разных блоках) - не должен случаться
        name = f"{name} ({k['row']},{k['col']})"
    seen.add(name)
    keys.append((name, k["led"], k["row"], k["col"]))
keys.sort(key=lambda t: (t[2], t[3]))

z = km["zones"]
out = ["// Сгенерировано gen_layout.py из patch/keymap.json. Не править руками.", "#pragma once", "",
       "struct AULAKey { const char* name; unsigned char led; unsigned char row; unsigned char col; };", "",
       "#define AULA_MATRIX_ROWS 6", "#define AULA_MATRIX_COLS 21", "", "static const AULAKey aula_keys[] =", "{"]
out += [f'    {{ "Key: {n}", {l}, {r}, {c} }},' for n, l, r, c in keys]
out += ["};", "", f"#define AULA_KEY_COUNT {len(keys)}", ""]
def arr(name, leds):
    return f"static const unsigned char {name}[] = {{ {', '.join(map(str, leds))} }};"
out += [arr("aula_arrow_bar_leds", z["arrow_bar"]["leds"]), arr("aula_side_left_leds", z["side_left"]["leds"]),
        arr("aula_side_right_leds", z["side_right"]["leds"]), ""]
out += ["/* Порядок каналов (1 = красный и зелёный поменяны местами, GRB):",
        "   - зона над стрелками: GRB (проверено вручную)",
        "   - боковые полосы: обычный RGB (проверено в OpenRGB)",
        "   - клавиши: RGB */",
        "#define AULA_KEYS_SWAP_RG       0", "#define AULA_ARROW_BAR_SWAP_RG  1", "#define AULA_SIDES_SWAP_RG      0", ""]
(Path(__file__).parent / "Controllers" / "AULAF108ProController" / "AULAF108ProLayout.h").write_text("\n".join(out), encoding="utf-8")
print("клавиш:", len(keys), " матрица 6x21")
