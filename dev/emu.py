"""Эмулятор прошивки AULA F108 Pro (SN32F290, Cortex-M0) на Unicorn.

python emu.py [образ.bin] [макс_инструкций]

Флеш 0x0, RAM 0x20000000, периферия 0x40000000+ - заглушки с журналом обращений.
Цель: довести прошивку до главного цикла, потом подставлять FN-нажатия и смотреть буферы LED.
"""
import sys
from collections import Counter
from pathlib import Path

from unicorn import (UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE,
                     UC_MODE_MCLASS, UC_MODE_THUMB, Uc, UcError)
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_MSP, UC_ARM_REG_PC, UC_ARM_REG_R0,
                               UC_ARM_REG_SP)

IMG = Path("stock_image.bin")   # stock firmware image (RCDATA/4000 of the official updater), not included
FLASH, FLASH_SZ = 0x0, 0x40000
RAM, RAM_SZ = 0x20000000, 0x10000
PERIPH, PERIPH_SZ = 0x40000000, 0x100000
LED_R, LED_G, LED_B, N_LED = 0x20001842, 0x20001968, 0x20001A8E, 147


class Emu:
    def __init__(self, image=IMG):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        uc = self.uc
        self.image = bytearray(Path(image).read_bytes())
        uc.mem_map(FLASH, FLASH_SZ)
        uc.mem_write(FLASH, bytes(self.image))
        uc.mem_map(RAM, RAM_SZ)
        uc.mem_map(PERIPH, PERIPH_SZ)
        uc.mem_map(0xE0000000, 0x100000)  # SCS/SysTick/NVIC
        self.periph_log = Counter()
        # адрес -> значение при чтении (имитация готовности PLL/тактов и т.п.)
        self.periph_ret = {0x40060008: 0x40, 0x4006000C: 0x40, 0x40054014: 0x20}   # 0x40054014: UART "передача завершена"
        uc.hook_add(UC_HOOK_MEM_READ, self._pr, begin=PERIPH, end=PERIPH + PERIPH_SZ - 1)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._pw, begin=PERIPH, end=PERIPH + PERIPH_SZ - 1)
        self.self_clear = {a: 0x2 for a in range(0x40000000, 0x40010000, 0x2000)}
        self._pending = None
        uc.hook_add(UC_HOOK_CODE, self._tick)
        self.icount = 0
        self.pcs = Counter()

    def _pr(self, uc, access, addr, size, value, _):
        self.periph_log[("R", addr)] += 1
        if addr in self.periph_ret:
            uc.mem_write(addr, self.periph_ret[addr].to_bytes(size, "little"))

    def _pw(self, uc, access, addr, size, value, _):
        self.periph_log[("W", addr)] += 1
        # регистры с самосбрасывающимся битом (запрос сброса/старта): считаем, что операция уже завершена
        mask = self.self_clear.get(addr)
        if mask is not None:
            self._pending = (addr, size, value & ~mask)

    def _tick(self, uc, addr, size, _):
        # применяем отложенную запись на следующей инструкции (после того как store отработал)
        if self._pending:
            a, sz, v = self._pending
            self._pending = None
            uc.mem_write(a, (v & ((1 << 8 * sz) - 1)).to_bytes(sz, "little"))

    def reset(self):
        sp = int.from_bytes(self.image[0:4], "little")
        pc = int.from_bytes(self.image[4:8], "little")
        self.uc.reg_write(UC_ARM_REG_SP, sp)
        self.pc = pc & ~1
        return self.pc

    def run(self, max_insn=2_000_000, start=None):
        pc = start if start is not None else self.pc
        try:
            self.uc.emu_start(pc | 1, 0, count=max_insn)
        except UcError as e:
            print("UcError:", e, "PC=0x%X" % self.uc.reg_read(UC_ARM_REG_PC))
        return self.uc.reg_read(UC_ARM_REG_PC)

    def call_irq(self, handler, max_insn=200000):
        """Вызвать обработчик прерывания как функцию: сохранить регистры, выполнить до возврата, восстановить."""
        from unicorn.arm_const import UC_ARM_REG_R0 as R0, UC_ARM_REG_R12, UC_ARM_REG_LR as LR
        uc = self.uc
        regs = [R0 + i for i in range(13)] + [UC_ARM_REG_SP, LR]
        saved = [uc.reg_read(r) for r in regs]
        pc = uc.reg_read(UC_ARM_REG_PC)
        MAGIC = 0x0003FFF0
        uc.reg_write(LR, MAGIC | 1)
        try:
            uc.emu_start(handler | 1, MAGIC, count=max_insn)
        except UcError as e:
            print("IRQ UcError", e)
        for r, v in zip(regs, saved):
            uc.reg_write(r, v)
        uc.reg_write(UC_ARM_REG_PC, pc)

    def run_slices(self, slices, per_slice=20000, systick=None):
        for _ in range(slices):
            try:
                self.uc.emu_start(self.uc.reg_read(UC_ARM_REG_PC) | 1, 0, count=per_slice)
            except UcError as e:
                print("UcError:", e, "PC=0x%X" % self.uc.reg_read(UC_ARM_REG_PC))
                return False
            if systick:
                self.call_irq(systick)
        return True

    def leds(self):
        rd = lambda a: [int.from_bytes(self.uc.mem_read(a + 2 * i, 2), "little") for i in range(N_LED)]
        return rd(LED_R), rd(LED_G), rd(LED_B)


def main():
    img = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1].endswith(".bin") else IMG
    n = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 2_000_000
    e = Emu(img)
    print("reset PC=0x%X" % e.reset())
    pc = e.run(n)
    print("остановились: PC=0x%X" % pc)
    print("топ обращений к периферии:")
    for (k, a), c in e.periph_log.most_common(12):
        print("  %s 0x%08X x%d" % (k, a, c))


if __name__ == "__main__":
    main()
