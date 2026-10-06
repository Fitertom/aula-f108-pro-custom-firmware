// Сгенерировано gen_layout.py из patch/keymap.json. Не править руками.
#pragma once

struct AULAKey { const char* name; unsigned char led; unsigned char row; unsigned char col; };

#define AULA_MATRIX_ROWS 6
#define AULA_MATRIX_COLS 21

static const AULAKey aula_keys[] =
{
    { "Key: Escape", 0, 0, 0 },
    { "Key: F1", 2, 0, 2 },
    { "Key: F2", 3, 0, 3 },
    { "Key: F3", 4, 0, 4 },
    { "Key: F4", 5, 0, 5 },
    { "Key: F5", 6, 0, 6 },
    { "Key: F6", 7, 0, 7 },
    { "Key: F7", 8, 0, 8 },
    { "Key: F8", 9, 0, 9 },
    { "Key: F9", 10, 0, 10 },
    { "Key: F10", 11, 0, 11 },
    { "Key: F11", 12, 0, 12 },
    { "Key: F12", 13, 0, 13 },
    { "Key: Print Screen", 14, 0, 14 },
    { "Key: Scroll Lock", 15, 0, 15 },
    { "Key: Pause/Break", 16, 0, 16 },
    { "Key: `", 21, 1, 0 },
    { "Key: 1", 22, 1, 1 },
    { "Key: 2", 23, 1, 2 },
    { "Key: 3", 24, 1, 3 },
    { "Key: 4", 25, 1, 4 },
    { "Key: 5", 26, 1, 5 },
    { "Key: 6", 27, 1, 6 },
    { "Key: 7", 28, 1, 7 },
    { "Key: 8", 29, 1, 8 },
    { "Key: 9", 30, 1, 9 },
    { "Key: 0", 31, 1, 10 },
    { "Key: -", 32, 1, 11 },
    { "Key: =", 33, 1, 12 },
    { "Key: Backspace", 34, 1, 13 },
    { "Key: Insert", 35, 1, 14 },
    { "Key: Home", 36, 1, 15 },
    { "Key: Page Up", 37, 1, 16 },
    { "Key: Num Lock", 38, 1, 17 },
    { "Key: Number Pad /", 39, 1, 18 },
    { "Key: Number Pad *", 40, 1, 19 },
    { "Key: Number Pad -", 41, 1, 20 },
    { "Key: Tab", 42, 2, 0 },
    { "Key: Q", 43, 2, 1 },
    { "Key: W", 44, 2, 2 },
    { "Key: E", 45, 2, 3 },
    { "Key: R", 46, 2, 4 },
    { "Key: T", 47, 2, 5 },
    { "Key: Y", 48, 2, 6 },
    { "Key: U", 49, 2, 7 },
    { "Key: I", 50, 2, 8 },
    { "Key: O", 51, 2, 9 },
    { "Key: P", 52, 2, 10 },
    { "Key: [", 53, 2, 11 },
    { "Key: ]", 54, 2, 12 },
    { "Key: \\", 55, 2, 13 },
    { "Key: Delete", 56, 2, 14 },
    { "Key: End", 57, 2, 15 },
    { "Key: Page Down", 58, 2, 16 },
    { "Key: Number Pad 7", 59, 2, 17 },
    { "Key: Number Pad 8", 60, 2, 18 },
    { "Key: Number Pad 9", 61, 2, 19 },
    { "Key: Number Pad +", 62, 2, 20 },
    { "Key: Caps Lock", 63, 3, 0 },
    { "Key: A", 64, 3, 1 },
    { "Key: S", 65, 3, 2 },
    { "Key: D", 66, 3, 3 },
    { "Key: F", 67, 3, 4 },
    { "Key: G", 68, 3, 5 },
    { "Key: H", 69, 3, 6 },
    { "Key: J", 70, 3, 7 },
    { "Key: K", 71, 3, 8 },
    { "Key: L", 72, 3, 9 },
    { "Key: ;", 73, 3, 10 },
    { "Key: '", 74, 3, 11 },
    { "Key: Enter", 75, 3, 13 },
    { "Key: Number Pad 4", 80, 3, 17 },
    { "Key: Number Pad 5", 81, 3, 18 },
    { "Key: Number Pad 6", 82, 3, 19 },
    { "Key: Left Shift", 84, 4, 0 },
    { "Key: Z", 86, 4, 2 },
    { "Key: X", 87, 4, 3 },
    { "Key: C", 88, 4, 4 },
    { "Key: V", 89, 4, 5 },
    { "Key: B", 90, 4, 6 },
    { "Key: N", 91, 4, 7 },
    { "Key: M", 92, 4, 8 },
    { "Key: ,", 93, 4, 9 },
    { "Key: .", 94, 4, 10 },
    { "Key: /", 95, 4, 11 },
    { "Key: Right Shift", 96, 4, 13 },
    { "Key: Up Arrow", 99, 4, 15 },
    { "Key: Number Pad 1", 101, 4, 17 },
    { "Key: Number Pad 2", 102, 4, 18 },
    { "Key: Number Pad 3", 103, 4, 19 },
    { "Key: Number Pad Enter", 104, 4, 20 },
    { "Key: Left Control", 105, 5, 0 },
    { "Key: Left Windows", 106, 5, 1 },
    { "Key: Left Alt", 107, 5, 2 },
    { "Key: Space", 111, 5, 6 },
    { "Key: Right Alt", 115, 5, 10 },
    { "Key: Right Fn", 116, 5, 11 },
    { "Key: Menu", 117, 5, 12 },
    { "Key: Right Control", 118, 5, 13 },
    { "Key: Left Arrow", 119, 5, 14 },
    { "Key: Down Arrow", 120, 5, 15 },
    { "Key: Right Arrow", 121, 5, 16 },
    { "Key: Number Pad 0", 123, 5, 18 },
    { "Key: Number Pad .", 124, 5, 19 },
};

#define AULA_KEY_COUNT 104

static const unsigned char aula_arrow_bar_leds[] = { 76, 77, 78, 79, 83 };
static const unsigned char aula_side_left_leds[] = { 126, 127, 128, 129, 130, 131, 132 };
static const unsigned char aula_side_right_leds[] = { 133, 134, 135, 136, 137, 138, 139 };

/* Порядок каналов (1 = красный и зелёный поменяны местами, GRB):
   - зона над стрелками: GRB (проверено вручную)
   - боковые полосы: обычный RGB (проверено в OpenRGB)
   - клавиши: RGB */
#define AULA_KEYS_SWAP_RG       0
#define AULA_ARROW_BAR_SWAP_RG  1
#define AULA_SIDES_SWAP_RG      0
