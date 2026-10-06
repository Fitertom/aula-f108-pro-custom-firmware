"""Читает заряд AULA F108 Pro (только чтение). Работает в режиме донгла 2.4G.

python battery.py          -> заряд
python battery.py --list   -> список HID-интерфейсов клавиатуры
"""
import sys
import time

import hid

DEVICES = [(0x05AC, 0x024F), (0x0C45, 0x800A)]  # донгл 2.4G, USB-провод


def find():
    return [d for vid, pid in DEVICES for d in hid.enumerate(vid, pid)]


def packet(cmd, *args):
    buf = bytearray(65)  # [0] = report ID 0, дальше команда
    buf[1] = cmd
    for i, a in enumerate(args):
        buf[2 + i] = a
    buf[32] = sum(buf) & 0xFF  # 8-битная контрольная сумма, как в DeviceDriver.exe
    return bytes(buf)


def query(dev, pkt, tries=20):
    dev.write(pkt)
    for _ in range(tries):
        resp = dev.read(64, 100)
        # драйвер сверяет байты ответа 0..2 с байтами 1..3 отправленного
        if resp and bytes(resp[:3]) == pkt[1:4]:
            return bytes(resp)
        time.sleep(0.01)
    return None


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    found = find()
    if "--list" in sys.argv:
        for d in found:
            print(f"{d['vendor_id']:04X}:{d['product_id']:04X} mi={d['interface_number']} "
                  f"usage_page=0x{d['usage_page']:04X} usage=0x{d['usage']:02X} {d['path']}")
        return
    # vendor-интерфейс: usage_page >= 0xFF00 (драйвер ищет MI_03 на донгле)
    cands = [d for d in found if d["usage_page"] >= 0xFF00]
    if not cands:
        sys.exit("vendor HID-интерфейс не найден, смотрите --list")
    for d in cands:
        dev = hid.device()
        dev.open_path(d["path"])
        try:
            resp = query(dev, packet(0x20, 0x01))
        finally:
            dev.close()
        print(f"mi={d['interface_number']}: ", end="")
        if resp:
            print(f"ответ {resp[:8].hex(' ')}  заряд: {min(resp[3], 100)}%")
        else:
            print("нет ответа")


main()
