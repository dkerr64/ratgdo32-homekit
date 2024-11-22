/****************************************************************************
 * RATGDO HomeKit for ESP32
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-24 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 * Contributions acknowledged from
 * Brandon Matthews... https://github.com/thenewwazoo
 * Jonathan Stroud...  https://github.com/jgstroud
 *
 */
#pragma once

// C/C++ language includes
// none

// ESP system includes
// none

// RATGDO project includes
// none

void setup_web();
void web_loop();

enum BroadcastType : uint8_t
{
    RATGDO_STATUS = 1,
    LOG_MESSAGE = 2,
};
void SSEBroadcastState(const char *data, BroadcastType type = RATGDO_STATUS);

extern "C" int crashCount; // pull in number of times crashed.
