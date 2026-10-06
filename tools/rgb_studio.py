"""RGB Studio: ручное картирование и тест подсветки AULA F108 Pro (проводной режим, прошивка patch/custom.c).

python rgb_studio.py

Мышь:    ЛКМ по LED - покрасить текущим цветом (в режиме "кисть") или выбрать, ПКМ - погасить.
Клавиши: <-/-> предыдущий/следующий LED, Up/Down +-10, Space - вкл/выкл выбранный, Delete - погасить всё.
Режим "один LED": горит только выбранный (ходи стрелками и смотри, какой светодиод зажигается).
Раскладка каналов (RGB/GRB/...): что нужно слать на этот LED, чтобы цвет на клавиатуре совпал с выбранным.
Все заметки и раскладки каналов сохраняются в led_map.json.
"""
import json
import threading
import time
import tkinter as tk
from itertools import permutations
from pathlib import Path
from tkinter import colorchooser, ttk

import hid

HERE = Path(__file__).parent
KEYMAP = HERE / "keymap.json"
MAPFILE = HERE / "led_map.json"
N_LED = 147
PER_PACKET = 18
PERMS = ["".join(p) for p in permutations("RGB")]  # RGB, RBG, GRB, GBR, BRG, BGR

ZONES = [
    ("Зона над стрелками", [83, 79, 78, 77, 76]),
    ("Боковая полоса A (LED 126-132)", list(range(126, 133))),
    ("Боковая полоса B (LED 133-139)", list(range(133, 140))),
    ("LED 140-146 (неизвестно)", list(range(140, 147))),
    ("Матрица без клавиши в xml", [1, 17, 18, 19, 20, 85, 97, 98, 100, 108, 109, 110, 112, 113, 114, 122, 125]),
]


def hex2rgb(h):
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def rgb2hex(c):
    return "#%02x%02x%02x" % tuple(c)


def apply_perm(color, perm):
    d = dict(zip("RGB", color))
    return tuple(d[ch] for ch in perm)  # perm "GRB": на провод идёт G логический как первый канал и т.д.


class Device(threading.Thread):
    """Фоновый поток: шлёт кадры, пока окно открыто. Сам выбирает транспорт:
    провод (0C45:800A, интерфейс 0xFF13, feature report 'RGBD') или донгл 2.4G (05AC:024F, 0xFF60, команда 0x40)."""

    def __init__(self, get_frame):
        super().__init__(daemon=True)
        self.get_frame = get_frame
        self.stop = False
        self.status = "нет связи"
        self.dev = None
        self.kind = None
        self.last_frame = None
        self.last_sent = 0.0
        self.frames = 0
        self.t0 = time.time()

    def open(self):
        d = next((x for x in hid.enumerate(0x0C45, 0x800A) if x["usage_page"] == 0xFF13), None)
        kind = "wired"
        if d is None:
            d = next((x for x in hid.enumerate(0x05AC, 0x024F) if x["usage_page"] == 0xFF60), None)
            kind = "dongle"
        if d is None:
            raise OSError("клавиатура не найдена (ни провод 0C45:800A, ни донгл 05AC:024F)")
        dev = hid.device()
        dev.open_path(d["path"])
        self.kind = kind
        return dev

    @staticmethod
    def packet_wired(first, colors, flags=0):
        b = bytearray(65)
        b[1:5] = b"RGBD"
        b[5], b[6], b[7] = first, len(colors), flags
        for i, (r, g, bl) in enumerate(colors):
            b[9 + 3 * i:12 + 3 * i] = bytes((r, g, bl))
        return bytes(b)

    @staticmethod
    def packet_dongle(first, colors, flags=0):
        b = bytearray(65)
        b[1], b[2], b[3], b[4] = 0x40, first, len(colors), flags
        for i, (r, g, bl) in enumerate(colors):
            b[7 + 3 * i:10 + 3 * i] = bytes((r, g, bl))
        b[32] = sum(b) & 0xFF
        return bytes(b)

    def send_frame(self, frame):
        if self.kind == "wired":
            for first in range(0, N_LED, PER_PACKET):
                self.dev.send_feature_report(self.packet_wired(first, frame[first:first + PER_PACKET]))
        else:
            self.send_dongle_delta(frame)

    RADIO_GAP = 0.008      # радиоканал держит ~125 пакетов/с (замер); быстрее - пакеты теряются
    BLOCKS = [(i, min(i + 8, N_LED)) for i in range(0, N_LED, 8)]

    def send_dongle_delta(self, frame):
        """Только изменившиеся блоки по 8 LED; когда менять нечего - по одному блоку по кругу (лечит потери и
        обновляет таймер прошивки). Возвращает сколько пакетов ушло."""
        if not hasattr(self, "sent"):
            self.sent = [None] * N_LED
            self.rr = 0
        n = 0
        t_end = time.time() + 0.12            # не занимать поток надолго - вдруг цвета уже поменялись
        for a, b in self.BLOCKS:
            if frame[a:b] != self.sent[a:b]:
                self.dev.write(self.packet_dongle(a, frame[a:b]))
                self.sent[a:b] = frame[a:b]
                n += 1
                time.sleep(self.RADIO_GAP)
                if time.time() > t_end:
                    break
        if n == 0:
            a, b = self.BLOCKS[self.rr % len(self.BLOCKS)]
            self.rr += 1
            self.dev.write(self.packet_dongle(a, frame[a:b]))
            self.sent[a:b] = frame[a:b]
            n = 1
        return n

    def release(self):
        if self.kind == "wired":
            self.dev.send_feature_report(self.packet_wired(0, [], 0x80))
        else:
            self.dev.write(self.packet_dongle(0, [], 0x80))

    def run(self):
        while not self.stop:
            try:
                if self.dev is None:
                    self.dev = self.open()
                frame = self.get_frame()
                now = time.time()
                # по проводу шлём всегда; по радио - только изменения и поддерживающий кадр каждые 0.3 с
                if self.kind == "wired" or frame != self.last_frame or now - self.last_sent > 0.25:
                    self.send_frame(frame)
                    self.last_frame, self.last_sent = frame, now
                    self.frames += 1
                fps = self.frames / max(now - self.t0, 0.1)
                name = "ПРОВОД" if self.kind == "wired" else "ДОНГЛ 2.4G"
                self.status = f"{name}: связь есть, отправлено кадров {self.frames} (~{fps:.1f}/с)"
                time.sleep(0.03)
            except Exception as e:  # noqa: BLE001
                self.status = f"ошибка: {e}"
                try:
                    if self.dev:
                        self.dev.close()
                except Exception:  # noqa: BLE001
                    pass
                self.dev = None
                time.sleep(1)
        try:
            if self.dev:
                self.release()
                self.dev.close()
        except Exception:  # noqa: BLE001
            pass


class App:
    def __init__(self, root):
        self.root = root
        root.title("RGB Studio - AULA F108 Pro")
        self.lock = threading.Lock()
        self.colors = [(0, 0, 0)] * N_LED
        self.perm = ["RGB"] * N_LED
        self.note = [""] * N_LED
        self.sel = 0
        self.cur = (255, 255, 255)
        self.solo = tk.BooleanVar(value=True)
        self.solo_flag = True            # копия для фонового потока: к виджетам Tk из потока обращаться нельзя
        self.solo.trace_add("write", lambda *a: setattr(self, "solo_flag", bool(self.solo.get())))
        self.brush = tk.BooleanVar(value=False)
        self.load()
        self.build_ui()
        self.dev = Device(self.frame)
        self.dev.start()
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.bind("<Left>", lambda e: self.step(-1))
        root.bind("<Right>", lambda e: self.step(1))
        root.bind("<Up>", lambda e: self.step(10))
        root.bind("<Down>", lambda e: self.step(-10))
        root.bind("<space>", lambda e: self.toggle_sel())
        root.bind("<Delete>", lambda e: self.clear_all())
        self.select(0)
        self.tick()

    # ---------- данные ----------
    def load(self):
        if MAPFILE.exists():
            try:
                data = json.loads(MAPFILE.read_text(encoding="utf-8"))
                for k, v in data.get("leds", {}).items():
                    i = int(k)
                    self.perm[i] = v.get("perm", "RGB")
                    self.note[i] = v.get("note", "")
            except Exception:  # noqa: BLE001
                pass

    def save(self):
        data = {"leds": {str(i): {"perm": self.perm[i], "note": self.note[i]} for i in range(N_LED)
                         if self.perm[i] != "RGB" or self.note[i]}}
        MAPFILE.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")

    def frame(self):
        with self.lock:
            cols = list(self.colors)
            solo = self.solo_flag
            sel = self.sel
        out = []
        for i in range(N_LED):
            c = cols[i] if (not solo or i == sel) else (0, 0, 0)
            out.append(apply_perm(c, self.perm[i]))
        return out

    # ---------- интерфейс ----------
    def build_ui(self):
        top = ttk.Frame(self.root)
        top.pack(fill="x", padx=8, pady=6)
        self.status = ttk.Label(top, text="")
        self.status.pack(side="left")
        ttk.Button(top, text="Погасить всё (Del)", command=self.clear_all).pack(side="right")
        ttk.Button(top, text="Залить текущим", command=self.fill_all).pack(side="right", padx=4)

        body = ttk.Frame(self.root)
        body.pack(fill="both", expand=True, padx=8)
        self.canvas = tk.Canvas(body, width=1040, height=520, bg="#1c1c1c", highlightthickness=0)
        self.canvas.pack(side="left")
        self.items = {}      # led -> canvas id
        self.labels = {}
        self.draw_keys()
        self.draw_zones()
        self.canvas.bind("<Button-1>", lambda e: self.click(e, False))
        self.canvas.bind("<Button-3>", lambda e: self.click(e, True))
        self.canvas.bind("<B1-Motion>", lambda e: self.click(e, False, drag=True))

        side = ttk.Frame(body)
        side.pack(side="left", fill="y", padx=10)
        self.info = ttk.Label(side, text="", font=("Segoe UI", 11, "bold"), justify="left")
        self.info.pack(anchor="w")
        ttk.Checkbutton(side, text="Режим «один LED» (горит только выбранный)", variable=self.solo).pack(anchor="w", pady=4)
        ttk.Checkbutton(side, text="Кисть (ЛКМ красит, ПКМ гасит)", variable=self.brush).pack(anchor="w")
        self.swatch = tk.Label(side, text="  цвет  ", bg=rgb2hex(self.cur), width=14, height=2, relief="ridge")
        self.swatch.pack(anchor="w", pady=6)
        ttk.Button(side, text="Выбрать цвет...", command=self.pick).pack(anchor="w")
        row = ttk.Frame(side)
        row.pack(anchor="w", pady=4)
        for name, c in (("R", (255, 0, 0)), ("G", (0, 255, 0)), ("B", (0, 0, 255)), ("Белый", (255, 255, 255))):
            ttk.Button(row, text=name, width=6, command=lambda c=c: self.set_color(c)).pack(side="left", padx=2)

        ttk.Separator(side).pack(fill="x", pady=8)
        self.permlabel = ttk.Label(side, text="")
        self.permlabel.pack(anchor="w")
        row = ttk.Frame(side)
        row.pack(anchor="w", pady=4)
        ttk.Button(row, text="Сменить раскладку каналов", command=lambda: self.cycle_perm(False)).pack(side="left")
        ttk.Button(row, text="…для всей зоны", command=lambda: self.cycle_perm(True)).pack(side="left", padx=4)
        ttk.Label(side, text="Если шлёшь красный (R), а светится не красный - листай раскладку,\n"
                             "пока цвет на клавиатуре не совпадёт.", foreground="#666").pack(anchor="w")

        ttk.Separator(side).pack(fill="x", pady=8)
        ttk.Label(side, text="Заметка к LED (что это за светодиод):").pack(anchor="w")
        self.noteVar = tk.StringVar()
        e = ttk.Entry(side, textvariable=self.noteVar, width=36)
        e.pack(anchor="w")
        e.bind("<Return>", lambda ev: self.save_note())
        e.bind("<FocusOut>", lambda ev: self.save_note())
        ttk.Button(side, text="Сохранить заметку", command=self.save_note).pack(anchor="w", pady=4)

        ttk.Separator(side).pack(fill="x", pady=8)
        row = ttk.Frame(side)
        row.pack(anchor="w")
        ttk.Label(side, text="Перейти к LED №:").pack(anchor="w")
        self.gotoVar = tk.StringVar()
        g = ttk.Entry(side, textvariable=self.gotoVar, width=6)
        g.pack(anchor="w")
        g.bind("<Return>", lambda ev: self.goto())
        ttk.Label(side, text="←/→ листать LED, ↑/↓ на 10, Пробел вкл/выкл.", foreground="#666").pack(anchor="w", pady=8)

    def draw_keys(self):
        keys = json.loads(KEYMAP.read_text(encoding="utf-8"))["keys"]
        x0 = min(k["x"] for k in keys)
        y0 = min(k["y"] for k in keys)
        s = 1.12
        for k in keys:
            x, y = (k["x"] - x0) * s + 10, (k["y"] - y0) * s + 10
            w, h = k["w"] * s, k["h"] * s
            led = k["led"]
            if led is None or led >= N_LED:
                continue
            it = self.canvas.create_rectangle(x, y, x + w - 2, y + h - 2, fill="#2b2b2b", outline="#555")
            self.items[led] = it
            self.canvas.create_text(x + w / 2 - 1, y + h / 2 - 1, text=k["name"][:5], fill="#999", font=("Segoe UI", 7))
            self.canvas.tag_bind(it, "<Button-1>", lambda e, l=led: None)

    def draw_zones(self):
        y = 360
        for title, leds in ZONES:
            self.canvas.create_text(10, y, anchor="w", text=title, fill="#ccc", font=("Segoe UI", 9, "bold"))
            for n, led in enumerate(leds):
                x = 280 + n * 44
                it = self.canvas.create_oval(x, y - 14, x + 34, y + 20, fill="#2b2b2b", outline="#777")
                self.items[led] = it
                self.canvas.create_text(x + 17, y + 3, text=str(led), fill="#aaa", font=("Segoe UI", 8))
            y += 26
        self.canvas.create_text(10, 330, anchor="w", text="Зоны вне клавиатуры - нажми на круг, чтобы выбрать LED.", fill="#777")

    # ---------- действия ----------
    def led_at(self, x, y):
        for it in self.canvas.find_overlapping(x, y, x, y):
            for led, i in self.items.items():
                if i == it:
                    return led
        return None

    def click(self, e, erase, drag=False):
        led = self.led_at(e.x, e.y)
        if led is None:
            return
        if self.brush.get():
            with self.lock:
                self.colors[led] = (0, 0, 0) if erase else self.cur
            if not drag:
                self.select(led)
        else:
            self.select(led)
            if erase:
                with self.lock:
                    self.colors[led] = (0, 0, 0)
            else:
                with self.lock:
                    self.colors[led] = self.cur

    def select(self, led):
        self.save_note()
        self.sel = led % N_LED
        with self.lock:
            if self.colors[self.sel] == (0, 0, 0) and self.solo.get():
                self.colors[self.sel] = self.cur
        self.noteVar.set(self.note[self.sel])
        self.refresh_info()

    def step(self, d):
        if self.root.focus_get() and isinstance(self.root.focus_get(), (ttk.Entry, tk.Entry)):
            return
        self.select(self.sel + d)

    def toggle_sel(self):
        if isinstance(self.root.focus_get(), (ttk.Entry, tk.Entry)):
            return
        with self.lock:
            self.colors[self.sel] = (0, 0, 0) if self.colors[self.sel] != (0, 0, 0) else self.cur

    def goto(self):
        try:
            self.select(int(self.gotoVar.get()))
        except ValueError:
            pass

    def pick(self):
        c = colorchooser.askcolor(color=rgb2hex(self.cur))
        if c and c[0]:
            self.set_color(tuple(int(v) for v in c[0]))

    def set_color(self, c):
        self.cur = c
        self.swatch.config(bg=rgb2hex(c))
        with self.lock:
            self.colors[self.sel] = c

    def clear_all(self):
        with self.lock:
            self.colors = [(0, 0, 0)] * N_LED

    def fill_all(self):
        with self.lock:
            self.colors = [self.cur] * N_LED
        self.solo.set(False)

    def group_of(self, led):
        for _, leds in ZONES:
            if led in leds:
                return leds
        return [led]

    def cycle_perm(self, whole):
        nxt = PERMS[(PERMS.index(self.perm[self.sel]) + 1) % 6]
        for l in (self.group_of(self.sel) if whole else [self.sel]):
            self.perm[l] = nxt
        self.save()
        self.refresh_info()

    def save_note(self):
        v = self.noteVar.get().strip()
        if v != self.note[self.sel]:
            self.note[self.sel] = v
            self.save()

    def refresh_info(self):
        self.info.config(text=f"LED {self.sel}" + (f"   «{self.note[self.sel]}»" if self.note[self.sel] else ""))
        p = self.perm[self.sel]
        self.permlabel.config(text=f"Раскладка каналов LED {self.sel}: {p}   (логич. R,G,B -> на провод {p[0]},{p[1]},{p[2]})")

    def tick(self):
        self.status.config(text=self.dev.status)
        with self.lock:
            cols = list(self.colors)
            solo = self.solo.get()
        for led, it in self.items.items():
            c = cols[led] if (not solo or led == self.sel) else (0, 0, 0)
            self.canvas.itemconfig(it, fill=rgb2hex(c) if c != (0, 0, 0) else "#2b2b2b",
                                   outline="#ffffff" if led == self.sel else ("#777" if led >= 126 or led in (76, 77, 78, 79, 83) else "#555"),
                                   width=3 if led == self.sel else 1)
        self.root.after(80, self.tick)

    def close(self):
        self.save_note()
        self.save()
        self.dev.stop = True
        self.dev.join(timeout=2)
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
