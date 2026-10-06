"""Модель экрана ST7789 для эмулятора прошивки: перехват SPI-вывода и сборка кадра 240x135.

Перехват: FUN_00007808(cmd), FUN_00013974(byte) - все байты SPI, команда определяется флагом внутри FUN_00007808.
"""
from PIL import Image
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_LR, UC_ARM_REG_PC

W, H = 240, 135
CMD_FUN, SPI_FUN, CMD_RET = 0x7808, 0x13974, 0x781A


class LCD:
    def __init__(self, emu):
        self.emu = emu
        self.fb = bytearray(W * H * 2)            # RGB565 (big-endian как на шине)
        self.in_cmd = False
        self.cmd = 0
        self.params = []
        self.x0 = self.x1 = self.y0 = self.y1 = 0
        self.xoff = self.yoff = None
        self.pos = 0
        self.pending = None
        self.writes = 0
        self.log = []
        emu.uc.hook_add(UC_HOOK_CODE, self._hook)

    def _hook(self, uc, addr, size, _):
        if addr == CMD_FUN:
            self.in_cmd = True
            self._command(uc.reg_read(UC_ARM_REG_R0) & 0xFF)
        elif addr == CMD_RET:
            self.in_cmd = False
        elif addr == SPI_FUN and not self.in_cmd:
            self._data(uc.reg_read(UC_ARM_REG_R0) & 0xFF)

    def _command(self, c):
        self.cmd = c
        self.params = []
        if c == 0x2C:
            self.pos = 0
            self.pending = None
        self.log.append(c)

    def _data(self, b):
        c = self.cmd
        if c in (0x2A, 0x2B):
            self.params.append(b)
            if len(self.params) == 4:
                s = (self.params[0] << 8) | self.params[1]
                e = (self.params[2] << 8) | self.params[3]
                if c == 0x2A:
                    self.x0, self.x1 = s, e
                    if self.xoff is None or s < self.xoff: self.xoff = s
                else:
                    self.y0, self.y1 = s, e
                    if self.yoff is None or s < self.yoff: self.yoff = s
        elif c == 0x2C:
            if self.pending is None:
                self.pending = b
            else:
                px = (self.pending << 8) | b
                self.pending = None
                w = self.x1 - self.x0 + 1
                if w > 0:
                    x = self.x0 + self.pos % w
                    y = self.y0 + self.pos // w
                    xo, yo = self.xoff or 0, self.yoff or 0
                    xx, yy = x - xo, y - yo
                    if 0 <= xx < W and 0 <= yy < H:
                        i = (yy * W + xx) * 2
                        self.fb[i] = px >> 8
                        self.fb[i + 1] = px & 0xFF
                self.pos += 1
                self.writes += 1

    def image(self, scale=3):
        img = Image.new("RGB", (W, H))
        px = img.load()
        for y in range(H):
            for x in range(W):
                i = (y * W + x) * 2
                v = (self.fb[i] << 8) | self.fb[i + 1]
                px[x, y] = (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
        return img.resize((W * scale, H * scale), Image.NEAREST) if scale != 1 else img


class ExtFlash:
    """Модель внешней SPI-флеш (SPI1): чтение 0x03 через FUN_00006E24(addr) + FUN_00006DDC(byte).
    Содержимое задаёт fn(addr) -> байт (по умолчанию - узор по адресу, чтобы на скриншоте было видно, какой это адрес)."""
    READ_CMD, XFER = 0x6E24, 0x6DDC

    def __init__(self, emu, fn=None, log=None):
        self.emu = emu
        self.addr = 0
        self.fn = fn or self.pattern
        self.reads = []
        emu.uc.hook_add(UC_HOOK_CODE, self._hook, begin=0x6DDC, end=0x6DDC)
        emu.uc.hook_add(UC_HOOK_CODE, self._set_addr, begin=0x6E24, end=0x6E24)

    @staticmethod
    def pattern(addr):
        blk = addr // 0xFD20                       # один кадр 240x135 RGB565 = 64800 байт
        off = addr % 0xFD20
        r, g, b = (blk * 53) % 32, (blk * 29) % 64, (blk * 97) % 32
        px = (r << 11) | (g << 5) | b
        return (px >> 8) if off % 2 == 0 else (px & 0xFF)

    def _set_addr(self, uc, addr, size, _):
        self.addr = uc.reg_read(UC_ARM_REG_R0)
        self.reads.append(self.addr)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def _hook(self, uc, addr, size, _):
        if addr == self.XFER:
            b = self.fn(self.addr)
            self.addr += 1
            uc.reg_write(UC_ARM_REG_R0, b)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))


class Blit:
    """Перехват FUN_00007B82(x, y, w, h, flash_addr) - DMA-вывод картинки из внешней флеш на экран."""
    FUN = 0x7B82

    def __init__(self, emu, lcd, flash):
        self.emu, self.lcd, self.flash = emu, lcd, flash
        self.calls = []
        emu.uc.hook_add(UC_HOOK_CODE, self._hook, begin=self.FUN, end=self.FUN)

    def _hook(self, uc, addr, size, _):
        from unicorn.arm_const import UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP
        x, y, w, h = (uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
        a = int.from_bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_SP), 4), "little")
        self.calls.append((x, y, w, h, a))
        fb, fn = self.lcd.fb, self.flash.fn
        for row in range(h):
            for col in range(w):
                if 0 <= x + col < W and 0 <= y + row < H:
                    o = a + (row * w + col) * 2
                    i = ((y + row) * W + (x + col)) * 2
                    fb[i] = fn(o)
                    fb[i + 1] = fn(o + 1)
        self.lcd.writes += w * h
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
