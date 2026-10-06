.syntax unified
.thumb
.global rx_stub
.thumb_func
rx_stub:                        @ вызывается вместо "ldr r1,=0x20000040; movs r2,#1" по адресу 0x1A28
    push {r4, r5, r6, r7, lr}
    mov  r0, r4                 @ r4 = 0x20001340 (буфер приёма USB)
    bl   rx_handler
    cmp  r0, #0
    bne  1f
    pop  {r4, r5, r6, r7}       @ не наш пакет: выполнить то, что заменили, и продолжить как раньше
    pop  {r3}
    ldr  r1, =0x20000040
    movs r2, #1
    bx   r3
1:  pop  {r4, r5, r6, r7}       @ наш пакет: пропустить заводскую обработку (переход на 0x19EA)
    pop  {r3}
    ldr  r3, =0x000019EB
    bx   r3
.ltorg
