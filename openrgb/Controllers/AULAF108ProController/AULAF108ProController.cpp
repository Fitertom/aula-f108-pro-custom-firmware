/*---------------------------------------------------------*\
| AULAF108ProController.cpp                                 |
|                                                           |
|   Driver for AULA F108 Pro keyboard (wired USB) running   |
|   the custom "direct RGB" firmware                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <chrono>
#include <cstring>
#include "AULAF108ProController.h"
#include "StringUtils.h"

AULAF108ProController::AULAF108ProController(hid_device* dev_handle, const hid_device_info& info, std::string dev_name, AULATransport transport_type)
{
    transport   = transport_type;
    dev         = dev_handle;
    location    = info.path;
    name        = dev_name;
    active      = false;
    refresh_block = 0;

    memset(frame, 0, sizeof(frame));
    memset(block_dirty, 1, sizeof(block_dirty));

    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        serial_number = "";
    }
    else
    {
        serial_number = StringUtils::wstring_to_string(serial_string);
    }

    stop_keepalive  = false;
    if(transport == AULA_TRANSPORT_DONGLE)
    {
        keepalive_thread = std::thread(&AULAF108ProController::DongleThread, this);
    }
    else
    {
        keepalive_thread = std::thread(&AULAF108ProController::KeepAliveThread, this);
    }
}

AULAF108ProController::~AULAF108ProController()
{
    stop_keepalive = true;

    if(keepalive_thread.joinable())
    {
        keepalive_thread.join();
    }

    Release();
    hid_close(dev);
}

std::string AULAF108ProController::GetDeviceLocation()
{
    return("HID: " + location);
}

std::string AULAF108ProController::GetNameString()
{
    return(name);
}

std::string AULAF108ProController::GetSerialString()
{
    return(serial_number);
}

AULATransport AULAF108ProController::GetTransport()
{
    return(transport);
}

void AULAF108ProController::SetLEDs(const unsigned char* rgb)
{
    std::lock_guard<std::mutex> lock(mtx);

    /*-----------------------------------------------------*\
    | Mark the 8-LED blocks that changed (all of them when   |
    | the controller was just activated)                    |
    \*-----------------------------------------------------*/
    for(unsigned int blk = 0; blk < sizeof(block_dirty); blk++)
    {
        unsigned int first = blk * AULA_DONGLE_LEDS_PER_PACKET;
        unsigned int count = AULA_LED_COUNT - first;

        if(count > AULA_DONGLE_LEDS_PER_PACKET)
        {
            count = AULA_DONGLE_LEDS_PER_PACKET;
        }

        if(!active || memcmp(&frame[first * 3], &rgb[first * 3], count * 3) != 0)
        {
            block_dirty[blk] = true;
        }
    }

    memcpy(frame, rgb, sizeof(frame));
    active = true;

    /*-----------------------------------------------------*\
    | Wired: send the whole frame right away.               |
    | Dongle: the radio thread sends only changed blocks.   |
    \*-----------------------------------------------------*/
    if(transport == AULA_TRANSPORT_WIRED)
    {
        SendFrameLocked();
    }
}

void AULAF108ProController::Release()
{
    std::lock_guard<std::mutex> lock(mtx);

    active      = false;

    if(transport == AULA_TRANSPORT_WIRED)
    {
        unsigned char usb_buf[65];
        memset(usb_buf, 0x00, sizeof(usb_buf));

        usb_buf[0] = 0x00;                  // report ID
        usb_buf[1] = 'R';
        usb_buf[2] = 'G';
        usb_buf[3] = 'B';
        usb_buf[4] = 'D';
        usb_buf[7] = 0x80;                  // flags: release

        hid_send_feature_report(dev, usb_buf, sizeof(usb_buf));
    }
    else
    {
        SendDongleBlockLocked(0, 0, 0x80);
    }
}

void AULAF108ProController::SendFrameLocked()
{
    unsigned char usb_buf[65];

    for(unsigned int first = 0; first < AULA_LED_COUNT; first += AULA_LEDS_PER_PACKET)
    {
        unsigned int count = AULA_LED_COUNT - first;

        if(count > AULA_LEDS_PER_PACKET)
        {
            count = AULA_LEDS_PER_PACKET;
        }

        memset(usb_buf, 0x00, sizeof(usb_buf));

        usb_buf[0] = 0x00;              // report ID
        usb_buf[1] = 'R';
        usb_buf[2] = 'G';
        usb_buf[3] = 'B';
        usb_buf[4] = 'D';
        usb_buf[5] = (unsigned char)first;
        usb_buf[6] = (unsigned char)count;
        usb_buf[7] = 0x00;

        memcpy(&usb_buf[9], &frame[first * 3], count * 3);

        hid_send_feature_report(dev, usb_buf, sizeof(usb_buf));
    }
}

void AULAF108ProController::KeepAliveThread()
{
    while(!stop_keepalive)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(AULA_KEEPALIVE_MS));

        std::lock_guard<std::mutex> lock(mtx);

        if(active)
        {
            SendFrameLocked();
        }
    }
}

void AULAF108ProController::SendDongleBlockLocked(unsigned int first, unsigned int count, unsigned char flags)
{
    unsigned char usb_buf[65];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0] = 0x00;                      // report ID
    usb_buf[1] = 0x40;                      // command: direct RGB
    usb_buf[2] = (unsigned char)first;
    usb_buf[3] = (unsigned char)count;
    usb_buf[4] = flags;

    if(count > 0)
    {
        memcpy(&usb_buf[7], &frame[first * 3], count * 3);
    }

    unsigned char sum = 0;

    for(unsigned int i = 0; i < sizeof(usb_buf); i++)
    {
        sum = (unsigned char)(sum + usb_buf[i]);
    }

    usb_buf[32] = sum;

    hid_write(dev, usb_buf, sizeof(usb_buf));
}

void AULAF108ProController::DongleThread()
{
    /*-----------------------------------------------------*\
    | The 2.4G link carries about 125 packets per second,   |
    | each packet holds 8 LEDs. Send only blocks that       |
    | changed, one packet per slot; when nothing changed,   |
    | refresh one block per keep-alive interval (this also  |
    | repairs lost packets and keeps the firmware timer).   |
    \*-----------------------------------------------------*/
    const unsigned int block_count  = (AULA_LED_COUNT + AULA_DONGLE_LEDS_PER_PACKET - 1) / AULA_DONGLE_LEDS_PER_PACKET;
    auto last_send                  = std::chrono::steady_clock::now();

    while(!stop_keepalive)
    {
        bool sent_something = false;

        {
            std::lock_guard<std::mutex> lock(mtx);

            if(active)
            {
                for(unsigned int blk = 0; blk < block_count; blk++)
                {
                    unsigned int first = blk * AULA_DONGLE_LEDS_PER_PACKET;
                    unsigned int count = AULA_LED_COUNT - first;

                    if(count > AULA_DONGLE_LEDS_PER_PACKET)
                    {
                        count = AULA_DONGLE_LEDS_PER_PACKET;
                    }

                    if(block_dirty[blk])
                    {
                        block_dirty[blk] = false;
                        SendDongleBlockLocked(first, count, 0x00);
                        sent_something = true;
                        last_send      = std::chrono::steady_clock::now();
                        break;
                    }
                }

                if(!sent_something)
                {
                    auto now = std::chrono::steady_clock::now();

                    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send).count() >= AULA_KEEPALIVE_MS)
                    {
                        unsigned int first = (refresh_block % block_count) * AULA_DONGLE_LEDS_PER_PACKET;
                        unsigned int count = AULA_LED_COUNT - first;

                        if(count > AULA_DONGLE_LEDS_PER_PACKET)
                        {
                            count = AULA_DONGLE_LEDS_PER_PACKET;
                        }

                        refresh_block++;
                        SendDongleBlockLocked(first, count, 0x00);
                        sent_something = true;
                        last_send      = now;
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sent_something ? AULA_DONGLE_GAP_MS : 3));
    }
}
