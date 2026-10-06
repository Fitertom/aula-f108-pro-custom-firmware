/*---------------------------------------------------------*\
| RGBController_AULAF108Pro.h                               |
|                                                           |
|   RGBController for AULA F108 Pro keyboard (wired USB)    |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "AULAF108ProController.h"

class RGBController_AULAF108Pro : public RGBController
{
public:
    RGBController_AULAF108Pro(AULAF108ProController* controller_ptr);
    ~RGBController_AULAF108Pro();

    void SetupZones();

    void DeviceUpdateLEDs();
    void DeviceUpdateZoneLEDs(int zone);
    void DeviceUpdateSingleLED(int led);

    void DeviceUpdateMode();

private:
    AULAF108ProController*      controller;
    std::vector<bool>           swap_rg;        // per OpenRGB LED index: send G,R,B instead of R,G,B
};
