/*---------------------------------------------------------*\
| AULAF108ProController.h                                   |
|                                                           |
|   Driver for AULA F108 Pro keyboard (wired USB) running   |
|   the custom "direct RGB" firmware (RAM only, no flash)   |
|                                                           |
|   Protocol (HID feature report, interface 3, 64 bytes):   |
|     [0..3] 'R' 'G' 'B' 'D'                                |
|     [4]    first LED index                                |
|     [5]    LED count (max 18)                             |
|     [6]    flags (0x80 = release, back to stock lighting) |
|     [8..]  R G B per LED                                  |
|   Without packets for ~1.3 s the keyboard falls back to   |
|   its own lighting, so a keep-alive thread resends.       |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <hidapi.h>
#include "RGBController.h"

#define AULA_LED_COUNT          147
#define AULA_LEDS_PER_PACKET    18          // wired: 64-byte feature report
#define AULA_DONGLE_LEDS_PER_PACKET 8       // 2.4G: 32-byte payload through the dongle
#define AULA_DONGLE_GAP_MS      8           // radio link carries ~125 packets/s, faster loses packets
#define AULA_KEEPALIVE_MS       250

enum AULATransport
{
    AULA_TRANSPORT_WIRED,                   // USB cable, interface 3, feature reports 'RGBD'
    AULA_TRANSPORT_DONGLE                   // 2.4G dongle 05AC:024F, interface 3, output reports, command 0x40
};

class AULAF108ProController
{
public:
    AULAF108ProController(hid_device* dev_handle, const hid_device_info& info, std::string dev_name, AULATransport transport_type);
    ~AULAF108ProController();

    std::string     GetDeviceLocation();
    std::string     GetNameString();
    std::string     GetSerialString();

    /*-----------------------------------------------------*\
    | rgb: AULA_LED_COUNT * 3 bytes, in device LED order,   |
    | channel order already adjusted for the hardware       |
    \*-----------------------------------------------------*/
    void            SetLEDs(const unsigned char* rgb);
    AULATransport   GetTransport();

    /*-----------------------------------------------------*\
    | Give lighting back to the keyboard's own firmware     |
    \*-----------------------------------------------------*/
    void            Release();

private:
    hid_device*     dev;
    std::string     location;
    std::string     name;
    std::string     serial_number;

    AULATransport   transport;
    std::mutex      mtx;
    unsigned char   frame[AULA_LED_COUNT * 3];
    bool            block_dirty[(AULA_LED_COUNT + AULA_DONGLE_LEDS_PER_PACKET - 1) / AULA_DONGLE_LEDS_PER_PACKET]; // dongle: block must be (re)sent
    unsigned int    refresh_block;                  // dongle: round-robin refresh position
    bool            active;

    std::atomic<bool>   stop_keepalive;
    std::thread         keepalive_thread;

    void            SendFrameLocked();
    void            SendDongleBlockLocked(unsigned int first, unsigned int count, unsigned char flags);
    void            KeepAliveThread();
    void            DongleThread();
};
