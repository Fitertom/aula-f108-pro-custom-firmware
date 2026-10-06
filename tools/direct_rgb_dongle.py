"""Прямой RGB через донгл 2.4G (прошивка patch/custom.c, команда 0x40, RAM-only).

Клавиатура должна быть переключена в режим 2.4G, донгл (05AC:024F) воткнут.
python direct_rgb_dongle.py zones             - тест всех зон (5 сек)
python direct_rgb_dongle.py rainbow [пауза]   - бегущая радуга; пауза между пакетами в мс (по умолчанию 2)
python direct_rgb_dongle.py solid R G B
python direct_rgb_dongle.py off               - вернуть заводскую подсветку
python direct_rgb_dongle.py fps [пауза]       - замер: сколько полных кадров в секунду уходит в донгл
Пакет (65 байт, report id 0): b[1]=0x40 b[2]=первый LED b[3]=число (<=8) b[4]=флаги (0x80 выход) b[7..]=R,G,B; b[32]=сумма
"""
import colorsys
import sys
import time

import hid

N_LED = 147
PER = 8
ARROW_BAR = (76, 77, 78, 79, 83)   # у этой зоны красный/зелёный поменяны местами (GRB)


def open_dev():
    d = next(x for x in hid.enumerate(0x05AC, 0x024F) if x["usage_page"] == 0xFF60)
    dev = hid.device()
    dev.open_path(d["path"])
    return dev


def packet(first, colors, flags=0):
    b = bytearray(65)
    b[1], b[2], b[3], b[4] = 0x40, first, len(colors), flags
    for i, (r, g, bl) in enumerate(colors):
        b[7 + 3 * i:10 + 3 * i] = bytes((r, g, bl))
    b[32] = sum(b) & 0xFF
    return bytes(b)


def wire(colors):
    out = list(colors)
    for i in ARROW_BAR:
        r, g, b = out[i]
        out[i] = (g, r, b)
    return out


def send_frame(dev, colors, delay):
    colors = wire(colors)
    for first in range(0, N_LED, PER):
        dev.write(packet(first, colors[first:first + PER]))
        if delay:
            time.sleep(delay)


def release(dev):
    dev.write(packet(0, [], 0x80))


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "zones"
    delay = float(sys.argv[2]) / 1000 if mode in ("rainbow", "fps") and len(sys.argv) > 2 else 0.002
    dev = open_dev()
    try:
        if mode == "zones":
            c = [(20, 20, 20)] * N_LED
            for i in range(126, 133): c[i] = (255, 0, 0)
            for i in range(133, 140): c[i] = (0, 0, 255)
            for i in ARROW_BAR: c[i] = (0, 255, 0)
            print("5 сек: боковые = красная/синяя, зона над стрелками = зелёная, остальное тускло-серое")
            t = time.time()
            while time.time() - t < 5:
                send_frame(dev, c, delay)
        elif mode == "rainbow":
            t0 = time.time()
            while True:
                ph = (time.time() - t0) * 0.5
                send_frame(dev, [tuple(int(255 * v) for v in colorsys.hsv_to_rgb((ph + i / N_LED) % 1, 1, 1)) for i in range(N_LED)], delay)
        elif mode == "solid":
            r, g, b = (int(x) for x in sys.argv[2:5])
            t = time.time()
            while time.time() - t < 5:
                send_frame(dev, [(r, g, b)] * N_LED, 0.002)
        elif mode == "fps":
            n, t = 0, time.time()
            while time.time() - t < 5:
                send_frame(dev, [(255, 0, 0) if n % 2 else (0, 0, 255)] * N_LED, delay)
                n += 1
            print(f"{n / 5:.1f} полных кадров/с на стороне ПК (пауза между пакетами {delay * 1000:.1f} мс)")
    except KeyboardInterrupt:
        pass
    finally:
        release(dev)
        dev.close()


if __name__ == "__main__":
    main()
