/*---------------------------------------------------------*\
| RGBController_AULAF108Pro.cpp                             |
|                                                           |
|   RGBController for AULA F108 Pro keyboard (wired USB)    |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include "RGBController_AULAF108Pro.h"
#include "AULAF108ProLayout.h"

/**------------------------------------------------------------------*\
    @name AULA F108 Pro
    @category Keyboard
    @type USB
    @save :x:
    @direct :white_check_mark:
    @effects :x:
    @detectors DetectAULAF108ProControllers
    @comment Needs the custom direct-RGB firmware. Zones: keys, bar above arrows, left and right side strips.
\*-------------------------------------------------------------------*/

RGBController_AULAF108Pro::RGBController_AULAF108Pro(AULAF108ProController* controller_ptr)
{
    controller                  = controller_ptr;

    name                        = controller->GetNameString();
    vendor                      = "AULA";
    type                        = DEVICE_TYPE_KEYBOARD;
    description                 = (controller->GetTransport() == AULA_TRANSPORT_DONGLE) ? "AULA F108 Pro via 2.4G dongle (custom direct RGB firmware)" : "AULA F108 Pro wired (custom direct RGB firmware)";
    location                    = controller->GetDeviceLocation();
    serial                      = controller->GetSerialString();

    mode Direct;
    Direct.name                 = "Direct";
    Direct.value                = 0;
    Direct.flags                = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode           = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    mode Stock;
    Stock.name                  = "Keyboard's own lighting";
    Stock.value                 = 1;
    Stock.flags                 = 0;
    Stock.color_mode            = MODE_COLORS_NONE;
    modes.push_back(Stock);

    SetupZones();
}

RGBController_AULAF108Pro::~RGBController_AULAF108Pro()
{
    Shutdown();

    delete controller;
}

void RGBController_AULAF108Pro::SetupZones()
{
    /*---------------------------------------------------------*\
    | Keyboard matrix zone                                      |
    \*---------------------------------------------------------*/
    zone keys_zone;
    keys_zone.name              = "Keyboard";
    keys_zone.type              = ZONE_TYPE_MATRIX;
    keys_zone.leds_count        = AULA_KEY_COUNT;
    keys_zone.leds_min          = AULA_KEY_COUNT;
    keys_zone.leds_max          = AULA_KEY_COUNT;

    unsigned int matrix[AULA_MATRIX_ROWS * AULA_MATRIX_COLS];

    for(unsigned int i = 0; i < AULA_MATRIX_ROWS * AULA_MATRIX_COLS; i++)
    {
        matrix[i] = 0xFFFFFFFF;
    }

    for(unsigned int i = 0; i < AULA_KEY_COUNT; i++)
    {
        matrix[aula_keys[i].row * AULA_MATRIX_COLS + aula_keys[i].col] = i;

        led new_led;
        new_led.name            = aula_keys[i].name;
        new_led.value           = aula_keys[i].led;
        leds.push_back(new_led);
        swap_rg.push_back(AULA_KEYS_SWAP_RG != 0);
    }

    keys_zone.matrix_map.Set(AULA_MATRIX_ROWS, AULA_MATRIX_COLS, matrix);
    zones.push_back(keys_zone);

    /*---------------------------------------------------------*\
    | Linear zones: bar above the arrow keys and side strips    |
    \*---------------------------------------------------------*/
    struct linear_def
    {
        const char*             name;
        const unsigned char*    leds_list;
        unsigned int            count;
        const char*             led_prefix;
        bool                    swap;
    };

    const linear_def linear_zones[] =
    {
        { "Arrow bar",   aula_arrow_bar_leds,  sizeof(aula_arrow_bar_leds),  "Arrow bar ",   AULA_ARROW_BAR_SWAP_RG != 0 },
        { "Left strip",  aula_side_left_leds,  sizeof(aula_side_left_leds),  "Left strip ",  AULA_SIDES_SWAP_RG != 0 },
        { "Right strip", aula_side_right_leds, sizeof(aula_side_right_leds), "Right strip ", AULA_SIDES_SWAP_RG != 0 },
    };

    for(const linear_def& def : linear_zones)
    {
        zone new_zone;
        new_zone.name           = def.name;
        new_zone.type           = ZONE_TYPE_LINEAR;
        new_zone.leds_count     = def.count;
        new_zone.leds_min       = def.count;
        new_zone.leds_max       = def.count;
        zones.push_back(new_zone);

        for(unsigned int i = 0; i < def.count; i++)
        {
            led new_led;
            new_led.name        = std::string(def.led_prefix) + std::to_string(i + 1);
            new_led.value       = def.leds_list[i];
            leds.push_back(new_led);
            swap_rg.push_back(def.swap);
        }
    }

    SetupColors();
}

void RGBController_AULAF108Pro::DeviceUpdateLEDs()
{
    if(active_mode == 1)
    {
        controller->Release();
        return;
    }

    unsigned char rgb[AULA_LED_COUNT * 3];
    memset(rgb, 0x00, sizeof(rgb));

    for(unsigned int i = 0; i < leds.size(); i++)
    {
        unsigned int dev_led = leds[i].value;

        if(dev_led >= AULA_LED_COUNT)
        {
            continue;
        }

        unsigned char r = RGBGetRValue(colors[i]);
        unsigned char g = RGBGetGValue(colors[i]);
        unsigned char b = RGBGetBValue(colors[i]);

        rgb[dev_led * 3 + 0] = swap_rg[i] ? g : r;
        rgb[dev_led * 3 + 1] = swap_rg[i] ? r : g;
        rgb[dev_led * 3 + 2] = b;
    }

    controller->SetLEDs(rgb);
}

void RGBController_AULAF108Pro::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AULAF108Pro::DeviceUpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AULAF108Pro::DeviceUpdateMode()
{
    DeviceUpdateLEDs();
}
