/*---------------------------------------------------------*\
| AULAF108ProControllerDetect.cpp                           |
|                                                           |
|   Detector for AULA F108 Pro keyboard (wired USB)         |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "DetectionManager.h"
#include "AULAF108ProController.h"
#include "RGBController_AULAF108Pro.h"

/*---------------------------------------------------------*\
| AULA F108 Pro (Sonix SN32F290), vendor config interface 3 |
\*---------------------------------------------------------*/
#define AULA_F108_PRO_VID       0x0C45
#define AULA_F108_PRO_PID       0x800A
#define AULA_F108_PRO_INTERFACE 3
#define AULA_F108_PRO_USAGE_PAGE 0xFF13
#define AULA_F108_PRO_USAGE     0x01

DetectedControllers DetectAULAF108ProControllers(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        AULAF108ProController*      controller      = new AULAF108ProController(dev, *info, name, AULA_TRANSPORT_WIRED);
        RGBController_AULAF108Pro*  rgb_controller  = new RGBController_AULAF108Pro(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

/*---------------------------------------------------------*\
| 2.4G dongle (05AC:024F), vendor interface 3               |
\*---------------------------------------------------------*/
#define AULA_F108_PRO_DONGLE_VID        0x05AC
#define AULA_F108_PRO_DONGLE_PID        0x024F
#define AULA_F108_PRO_DONGLE_INTERFACE  3
#define AULA_F108_PRO_DONGLE_USAGE_PAGE 0xFF60
#define AULA_F108_PRO_DONGLE_USAGE      0x61

DetectedControllers DetectAULAF108ProDongleControllers(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        AULAF108ProController*      controller      = new AULAF108ProController(dev, *info, name, AULA_TRANSPORT_DONGLE);
        RGBController_AULAF108Pro*  rgb_controller  = new RGBController_AULAF108Pro(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_IPU("AULA F108 Pro (2.4G)", DetectAULAF108ProDongleControllers, AULA_F108_PRO_DONGLE_VID, AULA_F108_PRO_DONGLE_PID, AULA_F108_PRO_DONGLE_INTERFACE, AULA_F108_PRO_DONGLE_USAGE_PAGE, AULA_F108_PRO_DONGLE_USAGE);
REGISTER_HID_DETECTOR_IPU("AULA F108 Pro", DetectAULAF108ProControllers, AULA_F108_PRO_VID, AULA_F108_PRO_PID, AULA_F108_PRO_INTERFACE, AULA_F108_PRO_USAGE_PAGE, AULA_F108_PRO_USAGE);
