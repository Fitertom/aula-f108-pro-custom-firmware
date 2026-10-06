# 1. Flashing the custom firmware

Read [05-recovery.md](05-recovery.md) before you start.

## What you need

- The AULA F108 Pro, connected with its USB cable directly to the PC (not through a hub if you can avoid it).
  The official updater flashes only over the cable. It does not work over Bluetooth or the 2.4G dongle.
- The official `AULA_F108_Pro_Firmware.exe` from the manufacturer's download page for the F108 Pro.
- Python 3.8+ on Windows.
- A charged keyboard, or at least a cable that supplies power. Do not flash on a nearly empty battery.

## Step 1: check that you have the right official file

The patch is made for one specific firmware image. The patcher reads the image out of your exe and compares its SHA-256
with the value in `firmware/patch_data.json` (`stock_image_sha256`). If it does not match, the patcher stops and writes
nothing. Do not try to force it: patching a different firmware version at fixed addresses would corrupt it.

You can also compare the SHA-256 of the whole exe with `stock_exe_sha256` in the same file (PowerShell:
`Get-FileHash .\AULA_F108_Pro_Firmware.exe`). A different exe may still contain the same image, the image check is what counts.

## Step 2: create the custom updater

```
python firmware\apply_patch.py C:\path\to\AULA_F108_Pro_Firmware.exe
```

Output: `AULA_F108_Pro_Firmware_CUSTOM.exe` in the same folder (use `-o name.exe` to choose another name).
The original file is not changed. The script verifies the written exe by reading the image back.

If you use another flashing tool that wants a raw image, add `--bin-out patched_image.bin`.

## Step 3: flash

1. Close the AULA driver software and anything else that talks to the keyboard (OpenRGB, other RGB tools).
2. Connect the keyboard with the USB cable. Select the wired mode if your unit has a connection switch.
3. Run `AULA_F108_Pro_Firmware_CUSTOM.exe`. It is the normal official updater window, carrying the patched image.
4. Press Start. Do not touch the keyboard or the cable until it says "Update successfully".
5. The keyboard restarts. The first start may take a moment.

After flashing, lighting modes and some settings are reset to their defaults, as after any firmware update. In the
author's case the GIFs stored in the keyboard stayed intact, but keep your own backup of anything important.

## Step 4: check

- The zone above the arrow keys shows a battery gradient (red to green).
- Turn the knob on the home screen: after the home screen you should see the SIDE LIGHT page. See [02-menu.md](02-menu.md).

## Going back to stock

Run the unmodified official `AULA_F108_Pro_Firmware.exe` the same way. That restores the original firmware completely.
