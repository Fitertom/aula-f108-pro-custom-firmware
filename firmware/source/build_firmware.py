"""Build the patched AULA F108 Pro firmware image from source (for developers).

    python build_firmware.py --stock stock_image.bin --out patched_image.bin

stock_image.bin is the RCDATA/4000 resource of the official updater (apply_patch.py can write
the raw patched image with --bin-out; a stock image can be saved with any resource extractor).
The ARM GNU toolchain (arm-none-eabi-gcc, ld, objcopy, nm) must be in PATH, or set ARM_GNU_BIN
to its bin folder. The result is byte-identical to what apply_patch.py produces from patch_data.json.

Patches (addresses come from the analysis of the official image):
  0x6600  FUN_00006600 -> zone_draw     arrow-bar zone: battery / stock / solid / breath / rainbow / off
  0x094C  BL FUN_0000E20C -> tick       1 kHz counter, then the original call
  0x0910  BL FUN_0000DDC2 -> render_all direct RGB mode skips the stock effects (+ NOPs 0x914..0x91B)
  0x091C  BL FUN_00010746 -> ui_tick    SIDE LIGHT / ARROWS menu pages
  0x2E84  knob press in volume mode: Mute -> Play/Pause
  0x4BC0  BL FUN_0000592A -> uart_hook  direct RGB over the 2.4G dongle
  0x1A28  ldr/movs -> BL rx_stub        USB SET_FEATURE hook for direct RGB over the cable
"""
import argparse
import os
import struct
import subprocess
from pathlib import Path

here = Path(__file__).parent
BASE = 0x1A600
CPU = ["-mcpu=cortex-m0", "-mthumb"]

ap = argparse.ArgumentParser()
ap.add_argument("--stock", required=True, help="stock firmware image (RCDATA/4000), 262144 bytes")
ap.add_argument("--out", default="patched_image.bin")
args = ap.parse_args()
BIN = os.environ.get("ARM_GNU_BIN", "")


def run(exe, *a):
    exe = os.path.join(BIN, exe) if BIN else exe
    return subprocess.check_output([exe, *map(str, a)], text=True)


build = here / "_build"
build.mkdir(exist_ok=True)
objs = []
for s, flags in (("custom.c", ["-Os", "-ffreestanding", "-nostdlib"]), ("stub.S", [])):
    o = build / (s + ".o")
    run("arm-none-eabi-gcc", *CPU, *flags, "-c", here / s, "-o", o)
    objs.append(o)
run("arm-none-eabi-ld", "-T", here / "link.ld", *objs, "-o", build / "custom.elf")
run("arm-none-eabi-objcopy", "-O", "binary", build / "custom.elf", build / "custom.bin")
sym = {l.split()[2]: int(l.split()[0], 16) for l in run("arm-none-eabi-nm", build / "custom.elf").splitlines()}
code = (build / "custom.bin").read_bytes()

img = bytearray(Path(args.stock).read_bytes())
assert len(img) == 0x40000, "unexpected image size"
assert all(b == 0xFF for b in img[BASE:BASE + len(code)]), "target area is not empty"
img[BASE:BASE + len(code)] = code


def bl(src, dst):  # Thumb-2 BL encoding
    off = dst - (src + 4)
    s = (off >> 24) & 1
    i1 = (off >> 23) & 1
    i2 = (off >> 22) & 1
    j1 = (~(i1 ^ s)) & 1
    j2 = (~(i2 ^ s)) & 1
    hi = 0xF000 | (s << 10) | ((off >> 12) & 0x3FF)
    lo = 0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF)
    return struct.pack("<HH", hi, lo)


img[0x6600:0x6608] = struct.pack("<HHI", 0x4800, 0x4700, sym["zone_draw"] | 1)
assert img[0x94C:0x950] == bl(0x94C, 0xE20C), "unexpected code at 0x94C"
img[0x94C:0x950] = bl(0x94C, sym["tick"])
assert img[0x910:0x914] == bl(0x910, 0xDDC2) and img[0x914:0x918] == bl(0x914, 0xEA9C) and img[0x918:0x91C] == bl(0x918, 0x6882)
img[0x910:0x914] = bl(0x910, sym["render_all"])
img[0x914:0x91C] = struct.pack("<HHHH", 0xBF00, 0xBF00, 0xBF00, 0xBF00)
assert img[0x91C:0x920] == bl(0x91C, 0x10746), "unexpected code at 0x91C"
img[0x91C:0x920] = bl(0x91C, sym["ui_tick"])
assert img[0x2E84] == 0xE2 and img[0x2E85] == 0x21, "unexpected code at 0x2E84"
img[0x2E84] = 0xCD
assert img[0x4BC0:0x4BC4] == bl(0x4BC0, 0x592A), "unexpected code at 0x4BC0"
img[0x4BC0:0x4BC4] = bl(0x4BC0, sym["uart_hook"])
assert img[0x1A28:0x1A2C].hex() == "34490122", "unexpected code at 0x1A28"
img[0x1A28:0x1A2C] = bl(0x1A28, sym["rx_stub"])

Path(args.out).write_bytes(img)
print("OK: %s, code %d bytes" % (args.out, len(code)))
