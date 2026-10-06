import sys, struct
from capstone import *
d=open('stock_image.bin','rb').read()
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
a,b=int(sys.argv[1],16),int(sys.argv[2],16)
for i in md.disasm(d[a:b],a):
    s=f"{i.address:05x}: {i.mnemonic:6} {i.op_str}"
    if i.mnemonic=='ldr' and 'pc' in i.op_str:
        off=int(i.op_str.split('#')[1].rstrip(']'),16) if '#' in i.op_str else 0
        t=((i.address+4)&~3)+off
        s+=f"   ; ={struct.unpack_from('<I',d,t)[0]:#x}"
    print(s)
