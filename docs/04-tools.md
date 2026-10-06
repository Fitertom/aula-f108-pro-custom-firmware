# 4. Python tools

All tools need Python 3.8+ and the `hid` module (`pip install hidapi`). They need the custom firmware.
Close OpenRGB and the AULA driver software before using them: two programs writing to one keyboard conflict.

| Tool | Purpose |
|---|---|
| `tools/direct_rgb.py` | Direct RGB over the USB cable. |
| `tools/direct_rgb_dongle.py` | Direct RGB over the 2.4G dongle. |
| `tools/rgb_studio.py` | Small Tk program: paint keys and zones, test the LED map (uses `tools/keymap.json`). |
| `tools/battery.py` | Reads the battery level (works with the dongle, or lists HID interfaces with `--list`). |

## direct_rgb.py (cable)

```
python direct_rgb.py zones            # 5 s test: side strips red and blue, arrow zone, other LEDs dim gray
python direct_rgb.py rainbow          # moving rainbow on all 147 LEDs, Ctrl+C to quit
python direct_rgb.py solid 255 0 0    # one color for 5 s
python direct_rgb.py off              # release direct mode
```

On exit the tool releases direct mode and the keyboard returns to its stock lighting. If a tool is killed without releasing,
the keyboard returns to stock lighting by itself after about 1.3 s without frames.

## direct_rgb_dongle.py (2.4G)

The keyboard must be switched to 2.4G mode with the dongle plugged in.

```
python direct_rgb_dongle.py zones
python direct_rgb_dongle.py rainbow [pause_ms]
python direct_rgb_dongle.py solid R G B
python direct_rgb_dongle.py fps [pause_ms]     # how many full frames per second the radio link carries
python direct_rgb_dongle.py off
```

## Notes on LED indices

The firmware has 147 LED slots. Keys use the indices from `keymap.json`. The arrow zone uses 76, 77, 78, 79, 83 (left to right)
and has red and green swapped. Side strips are 126 to 132 (left, far to near) and 133 to 139 (right, near to far).
Indices 140 to 146 have no physical LED.

Use these tools as examples for your own software; the packet formats are in [06-protocol.md](06-protocol.md).
Comments inside some scripts are in Russian.
