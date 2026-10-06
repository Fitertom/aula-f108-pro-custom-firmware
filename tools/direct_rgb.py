"""Прямой RGB по проводу для прошивки patch/custom.c (RAM-only, флеш не пишется).

python direct_rgb.py zones     - тест зон: боковые красная/синяя, зона над стрелками зелёная, LED 140-146 белые
python direct_rgb.py rainbow   - бегущая радуга по всем 147 LED (Ctrl+C - выход)
python direct_rgb.py solid R G B
python direct_rgb.py off       - выйти из прямого режима (вернуть заводскую подсветку)

Пакет: SET_FEATURE (65 байт: report id 0 + 64), 'RGBD', первый LED, число LED (<=18), флаги, R G B ...
"""
import colorsys
import sys
import time

import hid

N_LED = 147
PER_PACKET = 18


def open_dev():
    d = next(x for x in hid.enumerate(0x0C45, 0x800A) if x["usage_page"] == 0xFF13)
    dev = hid.device()
    dev.open_path(d["path"])
    return dev


def packet(first, colors, flags=0):
    b = bytearray(65)
    b[1:5] = b"RGBD"
    b[5], b[6], b[7] = first, len(colors), flags
    for i, (r, g, bl) in enumerate(colors):
        b[9 + 3 * i:12 + 3 * i] = bytes((r, g, bl))
    return bytes(b)


def send_frame(dev, colors):
    for first in range(0, N_LED, PER_PACKET):
        dev.send_feature_report(packet(first, colors[first:first + PER_PACKET]))


def release(dev):
    dev.send_feature_report(packet(0, [], 0x80))


def hold(dev, colors, seconds):
    t = time.time()
    while time.time() - t < seconds:
        send_frame(dev, colors)
        time.sleep(0.05)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "zones"
    dev = open_dev()
    try:
        if mode == "zones":
            c = [(20, 20, 20)] * N_LED
            for i in range(126, 133): c[i] = (255, 0, 0)
            for i in range(133, 140): c[i] = (0, 0, 255)
            for i in (83, 79, 78, 77, 76): c[i] = (0, 255, 0)
            for i in range(140, 147): c[i] = (255, 255, 255)
            print("5 сек: боковые = красная/синяя, зона над стрелками = зелёная, LED 140-146 = белые, остальное тускло-серое")
            hold(dev, c, 5)
        elif mode == "rainbow":
            t0 = time.time()
            while True:
                ph = (time.time() - t0) * 0.5
                send_frame(dev, [tuple(int(255 * v) for v in colorsys.hsv_to_rgb((ph + i / N_LED) % 1, 1, 1)) for i in range(N_LED)])
                time.sleep(0.02)
        elif mode == "solid":
            r, g, b = (int(x) for x in sys.argv[2:5])
            hold(dev, [(r, g, b)] * N_LED, 5)
        elif mode == "off":
            pass
    except KeyboardInterrupt:
        pass
    finally:
        release(dev)
        dev.close()


if __name__ == "__main__":
    main()
