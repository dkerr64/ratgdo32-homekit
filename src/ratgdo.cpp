
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

// C/C++ language includes

// Arduino includes
#include <LittleFS.h>

// RATGDO project includes
#include "ratgdo.h"
#include "config.h"
#include "utilities.h"
#include "comms.h"
#include "homekit.h"
#include "web.h"
#include "softAP.h"

// Logger tag
static const char *TAG = "ratgdo";

GarageDoor garage_door;

// Track our memory usage
uint32_t free_heap = (1024*1024);
uint32_t min_heap = (1024*1024);
unsigned long next_heap_check = 0;

bool status_done = false;
unsigned long status_timeout;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial port to open
    LittleFS.begin(true);
    Serial.printf("\n\n\n=== R A T G D O ===\n");

    load_all_config_settings();

    if (softAPmode)
    {
        start_soft_ap();
    }
    else
    {
        setup_homekit();
    }
}

void service_timer_loop()
{
    // Service the Obstruction Timer
    // TODO obstruction_timer();

    unsigned long current_millis = millis();

#ifdef NTP_CLIENT
    if (enableNTP && clockSet && lastRebootAt == 0)
    {
        lastRebootAt = time(NULL) - (current_millis / 1000);
        RINFO(TAG, "System boot time: %s", timeString(lastRebootAt));
    }
#endif

    // LED flash timer
    //led->flash();

    // Motion Clear Timer
    if (garage_door.motion && (current_millis > garage_door.motion_timer))
    {
        RINFO(TAG, "Motion Cleared");
        garage_door.motion = false;
        notify_homekit_motion();
    }

    // Check heap
    if (current_millis > next_heap_check)
    {
        next_heap_check = current_millis + 1000;
        free_heap = ESP.getFreeHeap();
        if (free_heap < min_heap)
        {
            min_heap = free_heap;
            RINFO(TAG, "Free heap dropped to %d", min_heap);
        }
    }
}

void loop()
{
    comms_loop();
    web_loop();
    soft_ap_loop();
    service_timer_loop();
}
