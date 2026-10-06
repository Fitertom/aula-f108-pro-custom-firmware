import sys
from emu import *
from lcd import LCD, ExtFlash, Blit
e = Emu('patched_image.bin'); e.reset(); lcd = LCD(e); xf = ExtFlash(e); bl = Blit(e, lcd, xf)
e.run(30_000_000)
vec = lambda n: int.from_bytes(e.image[n*4:n*4+4], 'little') & ~1
e.run_slices(300, 20000, systick=vec(15))
wr = lambda a, v: e.uc.mem_write(a, bytes([v]))
rd = lambda a: int.from_bytes(e.uc.mem_read(a, 1), 'little')
def post(ev):
    q = 0x20002129; base = 0x20000228
    i = rd(base + 2); wr(q + i, ev); i += 1; wr(base + 2, 0 if i > 0x31 else i)
wr(0x20000228+6, 1); wr(0x2000022e, 1); e.run_slices(300, 20000, systick=vec(15))
n=[0]
Z=[76,77,78,79,83]
def st():
    r,g,b=e.leds()
    return f"стр {rd(0x2000022e):#04x} my={rd(0x200001FC):#04x} ar={rd(0x200001FD)},{rd(0x200001FE)},{rd(0x20000202)},{rd(0x20000203)} led(R,G,B)=" + " ".join(f"{r[i]:3d},{g[i]:3d},{b[i]:3d}" for i in Z)
def step(name, ev, slices=250, shot=True):
    if ev is not None: post(ev)
    e.run_slices(slices, 20000, systick=vec(15))
    n[0]+=1
    if shot: lcd.image(2).save(f'shots/a{n[0]:02d}.png')
    print(f'{n[0]:02d} {name:32} {st()}')
step('старт',None,50)
step('вход (SIDE sel3)',9,400)
for i in range(3): step('9 -> влево',9)
step('9 за край -> ARROWS sel3',9,500)
for i in range(3): step('9 -> влево',9)
step('нажатие (edit MODE)',0xC)
for i in range(6): step('8: режим+1',8,300)
step('нажатие',0xC)
step('8 -> цвет',8)
step('нажатие: цвет не для заряда?',0xC)
step('8 -> скорость',8)
step('8 -> яркость',8)
step('9 x3 -> режим',9); step('9',9); step('9',9)
step('нажатие',0xC)
for i in range(2): step('8: режим+1 (стоковый,цвет)',8,300)
step('нажатие',0xC); step('8 -> цвет',8); step('нажатие',0xC); step('8 цвет+1',8); step('8 цвет+1',8); step('нажатие',0xC)
step('8 -> скорость',8); step('8 -> яркость',8)
step('8 за край -> SIDE sel0',8,500)
step('9 за левый край -> ARROWS',9,500)
for i in range(3): step('9',9)
step('9 -> за левый -> гифка',9,400)
step('8 -> ARROWS sel0',8,500)
