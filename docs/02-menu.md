# 2. Using the on-screen menu

The keyboard screen is navigated with the rotary knob. The stock firmware has a ring of pages:
home screen, then further pages, then the GIF page, and so on. The patch adds two pages between the home screen
and the GIF page:

```
home screen  ->  SIDE LIGHT  ->  ARROWS  ->  GIF  ->  ...
```

Turn the knob in the direction that moves from the home screen to the next page and you enter SIDE LIGHT.

## One screen, four circles

Both new pages show four circles with a short value below each one.

```
 SIDE LIGHT                      ARROWS
 EFFECT  COLOR  SPEED  BRIGHT    MODE    COLOR  SPEED  BRIGHT
```

- Turn the knob: the highlight moves from circle to circle. Keep turning in the direction you entered with and the
  highlight walks from the rightmost circle to the leftmost; one more step leaves the page for the next one in the ring.
  Turn the other way to walk back, and past the first circle you return to the previous page.
- Press the knob: the highlighted circle becomes active (its ring turns cyan, the title shows what you edit).
- Turn the knob while a circle is active: the value changes and is applied immediately.
- Press the knob again: back to choosing a circle.
- A gray circle with NONE means the current effect or mode has no such setting. Pressing it shows a short hint.

## SIDE LIGHT (two side strips)

| Circle | Values |
|---|---|
| EFFECT | the six stock side-light effects |
| COLOR | palette color 1 to 8, or AUTO. Effects 3 and 6 have no color choice (NONE) |
| SPEED | 1 to 6 |
| BRIGHT | 1 to 6 |

These pages call the same internal functions as the FN keys of the stock firmware, so the result is the same as changing
the side lights with FN keys (including whatever the stock firmware stores on its own).

## ARROWS (zone above the arrow keys)

| Mode | What it does | COLOR | SPEED | BRIGHT |
|---|---|---|---|---|
| BATTERY | battery gradient, blinking below 20 percent (default) | NONE | NONE | yes |
| STOCK | the factory behaviour of that zone | NONE | NONE | NONE |
| SOLID | one color | yes | NONE | yes |
| BREATH | breathing in one color | yes | yes | yes |
| RAINBOW | moving rainbow | NONE | yes | yes |
| OFF | zone off | NONE | NONE | NONE |

The ARROWS settings live only in RAM. After the keyboard is switched off or unplugged, the zone returns to BATTERY.
This was a deliberate choice to avoid writing flash on every change.

## Knob in volume mode

The FN key toggles the knob between menu navigation and PC volume (stock behaviour). In volume mode the knob turns the
volume and pressing it now sends Play/Pause (stock: Mute).

## Notes

- If you press a key combination that the stock firmware handles itself (for example FN plus an effect key) while a
  custom page is open, the menu closes and the stock screen takes over. This is intended.
- When the keyboard is in direct RGB mode (OpenRGB running), the zone above the arrows and the side strips are driven
  by the PC, so the menu settings are not visible until OpenRGB releases the keyboard.
