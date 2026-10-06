# 8. Research log: what was done, reverse engineered and found

Russian summary: [../docs_ru/08-research-log.md](../docs_ru/08-research-log.md)

## Goal

Get more out of the AULA F108 Pro than the vendor software allows: show the battery level on the LED zone above the arrow
keys, control all LEDs from OpenRGB without wearing out the keyboard's flash, and control the side light strips from the keyboard
screen. Constraint: the vendor only ships a flashing tool, no source, no SDK.

## Method

1. Looked at the vendor software: the Windows driver program and the firmware updater. The updater carries the firmware image
   as a resource (RCDATA 4000, 256 KB, flat Cortex-M0 image).
2. Disassembled and decompiled the driver and the firmware image with Ghidra to find the USB/HID commands, the UI state machine and
   the LED renderers.
3. Built an emulator of the MCU (Unicorn, with models of the LCD and the external SPI flash) so that every change could be
   run and its screen output inspected before it touched real hardware.
4. Patched the image in place (small hooks plus new code in free flash space) instead of rebuilding a firmware from scratch,
   because the stock image contains USB, wireless, key scanning and screen code that is not available as source.
5. Flashed with the official updater, tested on the keyboard, repeated.

## What was found

Hardware and firmware
- MCU Sonix SN32F290 (Cortex-M0, no hardware divide, 256 KB flash, about 12 KB of RAM at 0x20000000). Wired USB 0C45:800A, 2.4G
  dongle 05AC:024F.
- LED data is three 16-bit planes (R, G, B) of 147 entries in RAM. Stock renderers fill them every frame. Only 140 are real: indices
  140 to 146 have no LED.
- Zone above the arrow keys: LEDs 76, 77, 78, 79, 83, with red and green swapped (GRB). Side strips: 126 to 132 left, 133 to 139 right,
  plain RGB. Key LEDs follow a ROM table indexed by the layout file of the vendor software.
- The firmware image can be flashed in full only over the cable. The updater only works wired.
- The main loop feeds a hardware watchdog on each pass. Long drawing without feeding it reboots the keyboard (this was the cause
  of a crash when the first four-circle page was added).
- Page 0xE of the UI state machine is not a menu page: it is a key matrix test in the main loop. Using that number for a custom page makes
  the keyboard emit random key events (found the hard way, now avoided).
- Stock key handling rewrites the UI page for FN combinations, so a custom page must notice that and close.
- Several RAM bytes next to the ones the patch needs are real stock variables with non-zero defaults; free bytes were found by
  emulating and tracing accesses.
- The vendor protocol: HID with a simple sum checksum, no encryption. A command exists that reboots the chip into its bootloader.
- Radio link of the dongle: roughly 100 to 125 small packets per second without loss, about 14 ms round trip. That is what limits
  frame rate in 2.4G mode.

## What was built

- A battery gradient on the arrow zone with a blink below 20 percent (replaces the stock zone renderer, can fall back to it).
- Direct RGB mode: a hook in the USB handler (cable) and one in the UART command dispatcher (dongle) accept frames and
  write the LED planes directly, skipping the stock effects. Frames are not stored, so there is no flash wear. The mode ends after about 1.3 s
  without packets.
- Knob in FN volume mode sends Play/Pause instead of Mute.
- The SIDE LIGHT and ARROWS menu pages: four circles on one screen, drawing with the stock LCD primitives, state handled by an
  interceptor placed before the stock UI event processing. The ring order home, SIDE LIGHT, ARROWS, GIF is kept consistent in both directions.
- An OpenRGB controller (keys as a matrix, arrow bar and two strips as linear zones) for both transports, with per-block dirty
  tracking for the slow radio link.
- A browser mockup of the menu was used to agree on the design before writing the drawing code.

## Mistakes worth learning from

- Assumed the side strips had the same channel order as the arrow zone: they do not.
- Used UI page 0xE for the menu: random key presses.
- Drew a full screen without feeding the watchdog: reboot.
- Used division in firmware code on a CPU without a divide instruction (compiles with a library call that does not exist in the
  freestanding build) and operator precedence in text centering.
- Assumed a RAM byte was free because it was zero: stock sets some bytes late or rarely.

## What is not done or not known

- The format of the screen GIFs and menu images in the external flash is only partly understood; no tool for them is included.
- Removing languages from the menu images was considered and dropped: they are images in the external flash, not strings.
- Persistent (flash) storage of ARROWS settings was deliberately not implemented.
- Only one unit and one firmware version were tested.
