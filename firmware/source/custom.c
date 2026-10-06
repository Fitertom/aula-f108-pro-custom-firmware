/* Индикатор заряда в зоне над стрелками (5 LED: 76,77,78,79,83).
 * Заменяет FUN_00006600. Заряд (0..100) берётся из 0x200000F6 - его же отдаёт команда 0x20.
 * Градиент красный->жёлтый->зелёный по сегментам, последний сегмент горит частично,
 * при заряде < 20% вся полоса мигает. Счётчик времени (1 кГц) - 16 бит в 0x200001F8,
 * его увеличивает tick(), подцепленный вместо вызова FUN_0000E20C в SysTick-цепочке.
 */
#define LED_R ((volatile unsigned short *)0x20001842)
#define LED_G ((volatile unsigned short *)0x20001968)
#define LED_B ((volatile unsigned short *)0x20001A8E)
#define U8(a)  (*(volatile unsigned char *)(a))
#define U16(a) (*(volatile unsigned short *)(a))

#define BATTERY    0x200000F6
#define MODE       0x200000E7
#define TICKS      0x200001F8
#define LOW_PCT    20
#define DIM        10   /* яркость "пустых" сегментов, из 256 */

static const unsigned char zone_led[5] = {76, 77, 78, 79, 83}   /* слева направо */;

void tick(void)
{
    U16(TICKS)++;
    ((void (*)(void))0xE20D)();  /* оригинальный вызов, который мы подменили */
}

static void put(unsigned led, unsigned r, unsigned g, unsigned b)
{
    LED_R[led] = r;
    LED_G[led] = g;
    LED_B[led] = b;
}

/* Режимы зоны над стрелками (выбираются в меню ARROWS, живут только в RAM):
 * 0 заряд, 1 заводское поведение, 2 один цвет, 3 дыхание, 4 радуга, 5 выкл. Адреса - как в ui_section. */
#define AR_MODE   0x200001FD
#define AR_COLOR  0x200001FE
#define AR_SPEED  0x20000202
#define AR_BRIGHT 0x20000203

static const unsigned char bright_tab[6] = { 18, 45, 85, 135, 195, 255 };

static unsigned ar_shift(void)          /* 6..1: чем выше скорость, тем короче период */
{
    unsigned v = U8(AR_SPEED);
    v = v < 3 ? v + 3 : v - 3;
    return v > 5 ? 1 : 6 - v;
}

static unsigned ar_scale(void)
{
    unsigned v = U8(AR_BRIGHT);
    return bright_tab[v > 5 ? 5 : 5 - v];
}

static void zone_stock(void)            /* то же, что делал FUN_00006600 */
{
    if (U8(0x20000056) != 0) {
        unsigned n = U8(0x200000F5);
        for (unsigned i = 0; i < 5; i++) {
            unsigned led = U8(0x1598F + i);
            LED_R[led] = i < n ? 0xFF : 0;
            LED_G[led] = 0;
            LED_B[led] = 0;
        }
        return;
    }
    for (unsigned i = 0; i < 5; i++) {
        unsigned led = U8(0x1598F + i);
        if (U8(0x200001D9) == 0) {
            unsigned k = U8(0x200001D6) * U8(0x200001F3 + i);
            LED_R[led] = (k * U8(0x200001E9 + i)) >> 16;
            LED_G[led] = (k * U8(0x200001E4 + i)) >> 16;
            LED_B[led] = (k * U8(0x200001EE + i)) >> 16;
        } else {
            put(led, 0, 0, 0);
        }
    }
}

/* цвет r,g,b (0..255) с коэффициентом k (0..256) в светодиод зоны: у зоны каналы GRB */
static void zput(unsigned led, unsigned r, unsigned g, unsigned b, unsigned k)
{
    put(led, (g * k) >> 8, (r * k) >> 8, (b * k) >> 8);
}

static void wheel(unsigned h, unsigned *r, unsigned *g, unsigned *b)   /* h 0..767 */
{
    unsigned f = h & 255;
    unsigned seg = h >> 8;
    if (seg == 0)      { *r = 255 - f; *g = f;       *b = 0; }
    else if (seg == 1) { *r = 0;       *g = 255 - f; *b = f; }
    else               { *r = f;       *g = 0;       *b = 255 - f; }
}

void zone_draw(void)
{
    /* то же условие, что в оригинале: иначе подсветка выключена */
    int on = (U8(0x20000041) == 0 || U8(MODE) != 0 || U16(0x20000114) != 0) && U8(0x20000171) == 0;
    unsigned mode = U8(AR_MODE);

    if (!on || mode == 5) {
        for (int i = 0; i < 5; i++)
            put(zone_led[i], 0, 0, 0);
        return;
    }
    if (mode == 1) {
        zone_stock();
        return;
    }

    unsigned sc = ar_scale();
    unsigned kb = sc + (sc >> 7);                           /* 0..256 */

    if (mode == 2 || mode == 3) {
        unsigned c = U8(AR_COLOR);
        volatile unsigned char *t = (volatile unsigned char *)(0x1538C + (c > 7 ? 7 : c) * 3);
        unsigned k = kb;
        if (mode == 3) {
            unsigned ph = (U16(TICKS) >> ar_shift()) & 511;
            unsigned tri = ph < 256 ? ph : 511 - ph;
            k = (kb * (30 + ((tri * 226) >> 8))) >> 8;      /* не гаснем до нуля */
        }
        for (int i = 0; i < 5; i++)
            zput(zone_led[i], t[0], t[1], t[2], k);
        return;
    }

    if (mode == 4) {
        unsigned ph = (U16(TICKS) >> ar_shift()) & 255;
        for (int i = 0; i < 5; i++) {
            unsigned h = ph * 3 + i * 60, r, g, b;
            if (h >= 768) h -= 768;
            if (h >= 768) h -= 768;
            wheel(h, &r, &g, &b);
            zput(zone_led[i], r, g, b, kb);
        }
        return;
    }

    unsigned pct = U8(BATTERY);
    if (pct > 100)
        pct = 100;

    int blank = pct < LOW_PCT && ((U16(TICKS) >> 8) & 1);  /* ~2 Гц */
    unsigned level = (pct * 13107) >> 10;                 /* pct*12.8: 256 на сегмент */

    for (int i = 0; i < 5; i++) {
        unsigned t = i * 255 / 4;
        unsigned r = t <= 128 ? 255 : (255 - t) * 2;
        unsigned g = t < 128 ? t * 2 : 255;

        int seg = (int)level - i * 256;
        unsigned k = seg <= 0 ? DIM : seg >= 256 ? 256 : (unsigned)seg;
        if (k < DIM)
            k = DIM;
        if (blank)
            k = 0;
        k = (k * kb) >> 8;
        zput(zone_led[i], r, g, 0, k);
    }
}

/* ===================== Прямой RGB-режим (OpenRGB) =====================
 * Пакет: SET_FEATURE (64 байта) по проводу, перехватывается в USB-прерывании (FUN_000019B4) до
 * заводского разбора. Во флеш ничего не пишется - только буферы подсветки в RAM.
 *   [0..3] 'R','G','B','D'   [4] первый LED   [5] число LED (<=18)   [6] флаги (0x80 = выйти из режима)
 *   [8..]  R,G,B для каждого LED подряд.  LED 0..146 (клавиши, боковые полосы, зона над стрелками...)
 * Нет пакетов ~1.3 с -> заводская подсветка возвращается сама.
 */
#define DIRECT_FLAG  0x200001FA
#define DIRECT_STAMP 0x200001FB
#define N_LED        147
#define DIRECT_TIMEOUT 20                       /* в единицах по 64 мс */

static unsigned now64(void) { return (U16(TICKS) >> 6) & 0xFF; }

int rx_handler(volatile unsigned char *base)
{
    volatile unsigned char *p = base + 0x14;
    if (p[0] != 'R' || p[1] != 'G' || p[2] != 'B' || p[3] != 'D')
        return 0;
    unsigned first = p[4], n = p[5];
    if (p[6] & 0x80) {
        U8(DIRECT_FLAG) = 0;
        return 1;
    }
    if (n > 18 || first + n > N_LED)
        return 1;
    for (unsigned i = 0; i < n; i++)
        put(first + i, p[8 + 3 * i], p[9 + 3 * i], p[10 + 3 * i]);
    U8(DIRECT_STAMP) = now64();
    U8(DIRECT_FLAG) = 1;
    return 1;
}

static int direct_active(void)
{
    if (!U8(DIRECT_FLAG))
        return 0;
    if (((now64() - U8(DIRECT_STAMP)) & 0xFF) > DIRECT_TIMEOUT) {
        U8(DIRECT_FLAG) = 0;
        return 0;
    }
    return 1;
}

void render_all(void)
{
    if (direct_active()) {
        int on = (U8(0x20000041) == 0 || U8(MODE) != 0 || U16(0x20000114) != 0) && U8(0x20000171) == 0;
        if (!on)                                      /* сон/подсветка выключена - гасим всё */
            for (unsigned i = 0; i < N_LED; i++)
                put(i, 0, 0, 0);
        return;
    }
    ((void (*)(void))0xDDC3)();                       /* FUN_0000DDC2: клавиши и эффекты */
    ((void (*)(void))0xEA9D)();                       /* FUN_0000EA9C: боковые полосы */
    ((void (*)(void))0x6883)();                       /* FUN_00006882: зона над стрелками */
}


/* ================= Прямой RGB по 2.4G (через донгл) =================
 * Донгл передаёт в клавиатуру кадр по UART: [0]=0x81, [1..32]=payload (байты 1..32 хост-пакета), [33]=сумма.
 * Свою команду разбираем здесь, вместо вызова FUN_0000592A (диспетчер команд), всё остальное уходит в оригинал.
 * Хост-пакет (65 байт, report id 0):  b[1]=0x40  b[2]=первый LED  b[3]=число LED (<=8)  b[4]=флаги (0x80 - выйти)
 *                                       b[7..30] = R,G,B для каждого LED
 */
void uart_hook(void)
{
    volatile unsigned char *f = (volatile unsigned char *)0x2000263C;

    if (f[0] == 0x81 && f[1] == 0x40) {
        unsigned sum = 0;
        for (unsigned i = 0; i < 0x21; i++)
            sum += f[i];
        if ((sum & 0xFF) == f[0x21]) {
            if (f[4] & 0x80) {
                U8(DIRECT_FLAG) = 0;
            } else {
                unsigned first = f[2], n = f[3];
                if (n <= 8 && first + n <= N_LED) {
                    for (unsigned i = 0; i < n; i++)
                        put(first + i, f[7 + 3 * i], f[8 + 3 * i], f[9 + 3 * i]);
                    U8(DIRECT_STAMP) = now64();
                    U8(DIRECT_FLAG) = 1;
                }
            }
            U8(0x20001505) = 0x0B;      /* чтобы код после диспетчера не делал сброс (как для живой матрицы 0x0B) */
            return;
        }
    }
    ((void (*)(void))0x592B)();         /* FUN_0000592A: заводской диспетчер */
}


/* ================= Пункт меню экрана "боковые полосы" (4 кружка) =================
 * Кольцо меню (вращение колёсика): главный экран -> [SIDE LIGHT] -> гифка -> ... -> главный экран.
 * Перехватываем события очереди экрана (FUN_00010746) перед заводским разбором:
 *   главный экран + вращение вперёд (9)  -> наш пункт;   гифка + вращение назад (8) -> наш пункт.
 * Экран - четыре кружка: эффект, цвет, скорость, яркость (так же, как макет sidelight_menu_mockup.html).
 *   Вращение выбирает кружок, нажатие (0xC) входит в него, вращение меняет значение (применяется сразу),
 *   нажатие возвращает к выбору. Вращение за край уводит в соседний пункт кольца.
 * Наша "страница" = 14 (в заводской прошивке не используется). Состояние - один байт MY_MODE:
 *   0 - нас нет; иначе 0x80 | 0x20 (подсказка "нет цветов") | 0x10 (редактирование) | номер кружка 0..3.
 * Изменения применяются теми же функциями, что и FN-клавиши боковых полос заводской прошивки.
 */
#define UI_B(i)     U8(0x20000228 + (i))        /* [2]=запись [3]=чтение [4]=событие [6]=страница [8]=пункт */
#define QBUF(i)     U8(0x20002129 + (i))
#define SCR_STATE   U8(0x2000022E)
#define SCR_SEL     U8(0x20000230)
#define DMA_IDLE    U8(0x200002AC)
#define MY_MODE     0x200001FC
#define MY_PAGE     0x20  /* 14 нельзя: в заводской прошивке страница 0xE = тест матрицы клавиш (FUN_639e) */
#define SIDE_EFFECT 0x20000192                  /* эффект 0..5 (FN 0x9B) */
#define SIDE_COLOR  0x20000193                  /* индекс цвета (FN 0x9C), 8 = авто */
#define SIDE_SPEED  0x20000194                  /* скорость 0..5 (FN 0x9A) */
#define SIDE_BRIGHT 0x20000196                  /* яркость 0..5 (FN 0x99) */
#define SIDE_COLMAX 0x20000198                  /* максимальный индекс цвета у текущего эффекта */
#define SIDE_PALETTE 0x1538C                    /* 8 цветов RGB888 по 3 байта */
#define N_SIDE_FX   6

#define F_ACTIVE 0x80
#define F_MSG    0x20
#define F_EDIT   0x10
#define F_ITEM   0x40                           /* 0 - SIDE LIGHT, 1 - ARROWS */
#define AR_MODE   0x200001FD                    /* режим зоны над стрелками: 0 заряд, 1 заводской, 2 цвет, 3 дыхание, 4 радуга, 5 выкл */
#define AR_COLOR  0x200001FE                    /* индекс цвета палитры 0..7 */
#define AR_SPEED  0x20000202                    /* хранится как "значение ^3-сдвиг": 0 = средняя скорость */
#define AR_BRIGHT 0x20000203                    /* хранится как 5 - яркость: 0 = максимум */
#define N_AR_MODES 6

#define stock_ui()          ((void (*)(void))0x10747)()
#define stock_post(e)       ((void (*)(unsigned))0x10717)(e)
#define lcd_fill(c, x0, y0, x1, y1) ((void (*)(unsigned, int, int, int, int))0x7B21)(c, x0, y0, x1, y1)
#define lcd_char(x, y, ch, fg, bg, sz) ((void (*)(unsigned, unsigned, int, unsigned, unsigned, unsigned, int))0x12DD5)(x, y, ch, fg, bg, sz, 0)


/* Главный цикл прошивки на каждом проходе кормит сторожевой таймер. Наша отрисовка длится сотни миллисекунд,
 * поэтому кормим его сами перед каждым примитивом, иначе клавиатура перезагрузится. */
static void feed(void)
{
    *(volatile unsigned *)0x4001000C = 0x5AFA55AAu;
}

static void fill(unsigned c, int x0, int y0, int x1, int y1)
{
    feed();
    lcd_fill(c, x0, y0, x1, y1);
}

static void chr(int x, int y, int ch, unsigned fg, unsigned bg, unsigned sz)
{
    feed();
    lcd_char(x, y, ch, fg, bg, sz);
}

#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_GRAY     0x8410
#define C_DGRAY    0x4A8C
#define C_DISABLED 0x2988
#define C_CYAN     0x365F
#define C_IN       0x10A3      /* заливка кружка */
#define C_IN_SEL   0x1947      /* заливка выбранного кружка */

#define CIRC_Y 52
#define CIRC_R 22
static const unsigned char circ_x[4] = { 33, 91, 149, 207 };
static const char *const circ_label[4] = { "EFFECT", "COLOR", "SPEED", "BRIGHT" };
static const char *const edit_title[4] = { "EFFECT", "COLOR", "SPEED", "BRIGHTNESS" };
static const char *const ar_edit_title[4] = { "MODE", "COLOR", "SPEED", "BRIGHTNESS" };
static const char *const ar_circ_label[4] = { "MODE", "COLOR", "SPEED", "BRIGHT" };
static const char *const ar_name[N_AR_MODES] = { "BATTERY", "STOCK", "SOLID", "BREATH", "RAINBOW", "OFF" };

static void wait_dma(void)
{
    for (unsigned n = 0; DMA_IDLE == 0 && n < 400000; n++)
        if ((n & 0xFF) == 0)
            feed();
}

static unsigned slen(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}

static void text_c(int cx, int y, const char *s, unsigned fg, unsigned size)
{
    int x = cx - ((int)(slen(s) * (size >> 1)) >> 1);

    for (; *s; s++, x += (size >> 1))
        chr(x, y, *s, fg, C_BLACK, size);
}

static void disc(int cx, int cy, int r, unsigned color)
{
    int r2 = r * r;

    for (int dy = -r; dy <= r; dy++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= r2 - dy * dy)
            dx++;
        fill(color, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static unsigned fx_now(void)
{
    unsigned k = U8(SIDE_EFFECT);
    return k >= N_SIDE_FX ? 0 : k;
}

static unsigned pal565(unsigned i)
{
    volatile unsigned char *t = (volatile unsigned char *)SIDE_PALETTE;

    if (i >= 8)
        return C_WHITE;
    t += i * 3;
    return ((t[0] & 0xF8) << 8) | ((t[1] & 0xFC) << 3) | (t[2] >> 3);
}

static unsigned colors_off(void)
{
    unsigned m = U8(SIDE_COLMAX);
    return m == 0 || m >= 9;
}

static unsigned cur_item(void)
{
    return (U8(MY_MODE) & F_ITEM) ? 1 : 0;
}

static unsigned ar_speed(void)  { unsigned v = U8(AR_SPEED);  v = v < 3 ? v + 3 : v - 3; return v > 5 ? 5 : v; }
static unsigned ar_bright(void) { unsigned v = U8(AR_BRIGHT); return v > 5 ? 0 : 5 - v; }

/* кружок недоступен: у выбранного эффекта/режима такой настройки нет */
static unsigned circ_off(unsigned i)
{
    unsigned m;

    if (!cur_item())
        return i == 1 && colors_off();
    m = U8(AR_MODE);
    if (i == 1) return !(m == 2 || m == 3);
    if (i == 2) return !(m == 3 || m == 4);
    if (i == 3) return !(m == 0 || (m >= 2 && m <= 4));
    return 0;
}

static void draw_icon(unsigned i, int cx, int cy, unsigned bg, int on)
{
    unsigned c = on ? C_WHITE : C_GRAY;

    if (i == 0 && cur_item()) {                 /* режим стрелок: батарейка */
        fill(c, cx - 11, cy - 6, cx + 9, cy + 6);
        fill(bg, cx - 9, cy - 4, cx + 7, cy + 4);
        fill(c, cx + 10, cy - 3, cx + 12, cy + 3);
        fill(c, cx - 7, cy - 2, cx + 1, cy + 2);
    } else if (i == 0) {                        /* эффект: молния */
        fill(c, cx + 0, cy - 12, cx + 4, cy - 9);
        fill(c, cx - 2, cy - 8,  cx + 3, cy - 5);
        fill(c, cx - 4, cy - 4,  cx + 2, cy - 1);
        fill(c, cx - 6, cy,      cx + 6, cy + 2);
        fill(c, cx - 1, cy + 3,  cx + 3, cy + 6);
        fill(c, cx - 3, cy + 7,  cx + 1, cy + 10);
    } else if (i == 1) {                        /* цвет: диск текущего цвета (авто - полосы палитры) */
        if (circ_off(1)) {
            disc(cx, cy, 10, C_DISABLED);
        } else if (cur_item()) {
            disc(cx, cy, 10, pal565(U8(AR_COLOR)));
        } else if (U8(SIDE_COLOR) >= 8) {
            for (unsigned k = 0; k < 8; k++)
                fill(pal565(k), cx - 10 + (int)k * 20 / 8, cy - 10, cx - 10 + ((int)k + 1) * 20 / 8 - 1, cy + 10);
        } else {
            disc(cx, cy, 10, pal565(U8(SIDE_COLOR)));
        }
    } else if (i == 2) {                        /* скорость: ">>" */
        chr(cx - 12, cy - 8, '>', c, bg, 16);
        chr(cx - 2,  cy - 8, '>', c, bg, 16);
    } else {                                    /* яркость: солнце */
        disc(cx, cy, 5, c);
        fill(c, cx - 1, cy - 12, cx + 1, cy - 8);
        fill(c, cx - 1, cy + 8,  cx + 1, cy + 12);
        fill(c, cx - 12, cy - 1, cx - 8, cy + 1);
        fill(c, cx + 8,  cy - 1, cx + 12, cy + 1);
        fill(c, cx - 8, cy - 8, cx - 7, cy - 7);
        fill(c, cx + 7, cy - 8, cx + 8, cy - 7);
        fill(c, cx - 8, cy + 7, cx - 7, cy + 8);
        fill(c, cx + 7, cy + 7, cx + 8, cy + 8);
    }
}

static void value_str(unsigned i, char *out)   /* out >= 8 байт */
{
    unsigned v, total;

    out[0] = 0;
    if (circ_off(i)) {
        out[0] = 'N'; out[1] = 'O'; out[2] = 'N'; out[3] = 'E'; out[4] = 0;
        return;
    }
    if (cur_item()) {
        if (i == 0) {
            const char *n = ar_name[U8(AR_MODE) >= N_AR_MODES ? 0 : U8(AR_MODE)];
            unsigned k = 0;
            while (n[k] && k < 7) { out[k] = n[k]; k++; }
            out[k] = 0;
            return;
        }
        v = i == 1 ? U8(AR_COLOR) : i == 2 ? ar_speed() : ar_bright();
        total = i == 1 ? 8 : 6;
        if (v > 7) v = 7;
        out[0] = '1' + v; out[1] = '/'; out[2] = '0' + total; out[3] = 0;
        return;
    }
    if (i == 1 && U8(SIDE_COLOR) >= 8) {
        out[0] = 'A'; out[1] = 'U'; out[2] = 'T'; out[3] = 'O'; out[4] = 0;
        return;
    }
    if (i == 0)      { v = fx_now();            total = N_SIDE_FX; }
    else if (i == 1) { v = U8(SIDE_COLOR);      total = U8(SIDE_COLMAX) >= 8 ? 8 : U8(SIDE_COLMAX) + 1; }
    else if (i == 2) { v = U8(SIDE_SPEED);      total = 6; }
    else             { v = U8(SIDE_BRIGHT);     total = 6; }
    if (v > 8)
        v = 8;
    out[0] = '1' + v; out[1] = '/'; out[2] = '0' + total; out[3] = 0;
}

static void draw_circle(unsigned i, int clear_texts)
{
    unsigned mode = U8(MY_MODE);
    int selected = (mode & 3) == i;
    int edit = selected && (mode & F_EDIT);
    int dim = circ_off(i);
    int cx = circ_x[i];
    char v[8];

    wait_dma();
    if (clear_texts)
        fill(C_BLACK, cx - 28, 76, cx + 28, 108);
    disc(cx, CIRC_Y, CIRC_R, dim ? C_DISABLED : (edit ? C_CYAN : (selected ? C_WHITE : C_DGRAY)));
    disc(cx, CIRC_Y, CIRC_R - (selected ? 3 : 2), selected ? C_IN_SEL : C_IN);
    draw_icon(i, cx, CIRC_Y, selected ? C_IN_SEL : C_IN, selected && !dim);
    value_str(i, v);
    text_c(cx, 78, v, dim ? C_DISABLED : (selected ? C_WHITE : C_GRAY), 12);
    text_c(cx, 94, cur_item() ? ar_circ_label[i] : circ_label[i], selected ? C_WHITE : C_GRAY, 12);
}

static void draw_title(void)
{
    unsigned mode = U8(MY_MODE);

    wait_dma();
    fill(C_BLACK, 0, 0, 239, 29);
    if (mode & F_EDIT)
        text_c(120, 4, cur_item() ? ar_edit_title[mode & 3] : edit_title[mode & 3], C_CYAN, 24);
    else
        text_c(120, 4, cur_item() ? "ARROWS" : "SIDE LIGHT", C_WHITE, 24);
}

static void draw_hint(void)
{
    unsigned mode = U8(MY_MODE);

    wait_dma();
    fill(C_BLACK, 0, 112, 239, 134);
    if (mode & F_MSG)
        text_c(120, 118, "no setting for this effect", 0xFD20, 12);
    else if (mode & F_EDIT)
        text_c(120, 118, "turn: change   press: OK", C_GRAY, 12);
    else
        text_c(120, 118, "turn: select   press: open", C_GRAY, 12);
}

static void clear_screen(void)
{
    for (int y = 0; y < 135; y += 8)
        fill(C_BLACK, 0, y, 239, y + 7 > 134 ? 134 : y + 7);
}

static void draw_all(void)
{
    wait_dma();
    clear_screen();
    draw_title();
    for (unsigned i = 0; i < 4; i++)
        draw_circle(i, 0);
    draw_hint();
}

/* --- применение значений: те же функции, что у FN-клавиш боковых полос --- */
static void side_effect_set(unsigned k)         /* FN 0x9B, но скорость и яркость остаются прежними */
{
    unsigned char sp = U8(SIDE_SPEED), br = U8(SIDE_BRIGHT);

    U8(SIDE_EFFECT) = k;
    U8(SIDE_BRIGHT) = br;
    U8(SIDE_SPEED)  = sp;
    ((void (*)(void))0xE5C3)();     /* FUN_0000E5C2 */
    ((void (*)(void))0xE315)();     /* FUN_0000E314 */
    ((void (*)(void))0xEAF5)();     /* FUN_0000EAF4: цвет по умолчанию для эффекта */
    ((void (*)(void))0xE571)();     /* FUN_0000E570: запуск эффекта */
    ((void (*)(void))0xE13F)();     /* FUN_0000E13E: сохранить настройки */
}

static void side_color_set(unsigned c)          /* FN 0x9C */
{
    U8(SIDE_COLOR) = c;
    ((void (*)(void))0xE571)();
    ((void (*)(void))0xE13F)();
}

static void side_speed_set(unsigned s)          /* FN 0x9A */
{
    U8(SIDE_SPEED) = s;
    ((void (*)(void))0xE315)();
    ((void (*)(void))0xE13F)();
}

static void side_bright_set(unsigned b)         /* FN 0x99 */
{
    U8(SIDE_BRIGHT) = b;
    ((void (*)(void))0xE5C3)();
    ((void (*)(void))0xE13F)();
}

static void my_enter(unsigned item, unsigned sel)
{
    UI_B(6)   = MY_PAGE;
    UI_B(8)   = 10;
    SCR_STATE = MY_PAGE;
    SCR_SEL   = 10;
    U8(MY_MODE) = F_ACTIVE | (item ? F_ITEM : 0) | sel;
    draw_all();
}

static void my_leave(unsigned dir)
{
    /* возвращаем заводской автомат в исходную точку и отдаём ему то же вращение */
    U8(MY_MODE) = 0;
    if (dir == 9) {                 /* дальше по кольцу - гифка: как будто мы на главном экране */
        UI_B(6) = 1;  UI_B(8) = 0;  SCR_STATE = 1;  SCR_SEL = 0;
        stock_post(9);
    } else {                        /* назад - главный экран: как будто мы на гифке */
        UI_B(6) = 12; UI_B(8) = 9;  SCR_STATE = 12; SCR_SEL = 9;
        stock_post(8);
    }
    stock_ui();
}

static void my_event(unsigned ev)
{
    unsigned mode = U8(MY_MODE);
    unsigned sel  = mode & 3;

    if (mode & F_MSG) {                         /* подсказка "нет цветов" живёт до следующего события */
        mode &= ~F_MSG;
        U8(MY_MODE) = mode;
        draw_hint();
    }

    if (!(mode & F_EDIT)) {
        if (ev == 9 || ev == 8) {
            unsigned item = cur_item();

            if (ev == 9 && sel == 0) {              /* за левый край: SIDE LIGHT -> ARROWS -> гифка */
                if (item) my_leave(9); else my_enter(1, 3);
                return;
            }
            if (ev == 8 && sel == 3) {              /* за правый край: ARROWS -> SIDE LIGHT -> главный экран */
                if (item) my_enter(0, 0); else my_leave(8);
                return;
            }
            unsigned n = (ev == 8) ? sel + 1 : sel - 1;
            U8(MY_MODE) = F_ACTIVE | (item ? F_ITEM : 0) | n;
            draw_circle(sel, 1);
            draw_circle(n, 1);
        } else if (ev == 0xC) {
            if (circ_off(sel)) {
                U8(MY_MODE) = mode | F_MSG;
                draw_hint();
            } else {
                U8(MY_MODE) = mode | F_EDIT;
                draw_title();
                draw_circle(sel, 1);
                draw_hint();
            }
        }
        return;
    }

    /* редактирование выбранного кружка */
    if (ev == 0xC) {
        U8(MY_MODE) = mode & ~F_EDIT;
        draw_title();
        draw_circle(sel, 1);
        draw_hint();
    } else if ((ev == 9 || ev == 8) && cur_item()) {
        int up = (ev == 8);

        if (sel == 0) {
            unsigned m = U8(AR_MODE);
            m = up ? ((m + 1 >= N_AR_MODES) ? 0 : m + 1) : ((m == 0) ? N_AR_MODES - 1 : m - 1);
            U8(AR_MODE) = m;
            for (unsigned k = 0; k < 4; k++)
                draw_circle(k, 1);              /* у режима другие доступные настройки */
        } else if (sel == 1) {
            unsigned c = U8(AR_COLOR);
            U8(AR_COLOR) = up ? ((c >= 7) ? 0 : c + 1) : ((c == 0 || c > 7) ? 7 : c - 1);
            draw_circle(1, 1);
        } else if (sel == 2) {
            unsigned v = ar_speed();
            v = up ? ((v >= 5) ? 5 : v + 1) : ((v == 0) ? 0 : v - 1);
            U8(AR_SPEED) = v < 3 ? v + 3 : v - 3;
            draw_circle(2, 1);
        } else {
            unsigned v = ar_bright();
            v = up ? ((v >= 5) ? 5 : v + 1) : ((v == 0) ? 0 : v - 1);
            U8(AR_BRIGHT) = 5 - v;
            draw_circle(3, 1);
        }
    } else if (ev == 9 || ev == 8) {
        int up = (ev == 8);

        if (sel == 0) {
            unsigned k = fx_now();
            k = up ? ((k + 1 >= N_SIDE_FX) ? 0 : k + 1) : ((k == 0) ? N_SIDE_FX - 1 : k - 1);
            side_effect_set(k);
            draw_circle(0, 1);
            draw_circle(1, 1);                     /* у нового эффекта другой цвет по умолчанию */
        } else if (sel == 1) {
            unsigned c = U8(SIDE_COLOR), m = U8(SIDE_COLMAX);
            c = up ? ((c >= m) ? 0 : c + 1) : ((c == 0 || c > m) ? m : c - 1);
            side_color_set(c);
            draw_circle(1, 1);
        } else if (sel == 2) {
            unsigned s = U8(SIDE_SPEED);
            s = up ? ((s >= 5) ? 5 : s + 1) : ((s == 0) ? 0 : s - 1);
            side_speed_set(s);
            draw_circle(2, 1);
        } else {
            unsigned b = U8(SIDE_BRIGHT);
            b = up ? ((b >= 5) ? 5 : b + 1) : ((b == 0) ? 0 : b - 1);
            side_bright_set(b);
            draw_circle(3, 1);
        }
    }
}

void ui_tick(void)
{
    unsigned char rd = UI_B(3);

    /* заводской разбор клавиш (FN-комбинации и др.) сам переписывает страницу - тогда мы уходим, а не рисуем поверх */
    if ((U8(MY_MODE) & F_ACTIVE) && SCR_STATE != MY_PAGE)
        U8(MY_MODE) = 0;

    if (UI_B(2) != rd) {
        unsigned char ev   = QBUF(rd);
        unsigned char page = UI_B(6);
        int mine = (page == MY_PAGE) || (ev == 9 && page == 1) || (ev == 8 && page == 12);

        if (mine) {
            UI_B(4) = ev;
            UI_B(3) = (rd + 1 >= 0x32) ? 0 : rd + 1;
            if (page != MY_PAGE)
                my_enter(ev == 9 ? 0 : 1, ev == 9 ? 3 : 0);
            else
                my_event(ev);
            return;
        }
    }
    stock_ui();
}
