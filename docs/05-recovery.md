# 5. If something goes wrong

## Go back to the stock firmware

Run the original, unmodified `AULA_F108_Pro_Firmware.exe` with the keyboard connected by cable and press Start.
This writes the full stock image over the custom one. The custom firmware does not change the updater or the way it
communicates with the keyboard, so the same updater keeps working.

## The keyboard behaves strangely after flashing

1. Unplug the cable, wait a few seconds, plug it in again.
2. Flash the stock firmware (above) and check that the keyboard is fine.
3. Flash the custom firmware again.
4. If the problem repeats only with the custom firmware, open an issue and describe exactly what you did, with the exact
   firmware image hash printed by the patcher.

## The keyboard is not detected by the updater

- Use the cable, not Bluetooth or the dongle. Try another cable and another USB port, avoid hubs.
- Close the AULA driver software and OpenRGB.
- Look in Device Manager: a working keyboard shows up as USB ID 0C45:800A.

## Bricked keyboard

The patch only writes into free space of the application image and changes a few call instructions; the vector table and
the flashing mechanism are untouched. The author has not simulated a failed flash on purpose and cannot promise recovery in
every case. The Sonix chip has a bootloader that the official updater uses, and a keyboard whose application image does not start
normally can often still be reflashed through it, but this was not tested here.

Never send the command that reboots into the bootloader (`0x1F` in the vendor protocol) from your own tools unless you
know what you are doing.

## Things that are known to be limited

- Over 2.4G the radio link is slow, see [03-openrgb.md](03-openrgb.md).
- ARROWS menu settings are not saved across power cycles by design.
- Only the firmware image that matches `patch_data.json` is supported.
