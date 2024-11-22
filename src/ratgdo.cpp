
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

// Logger tag
static const char *TAG = "ratgdo";

GarageDoor garage_door;

// Track our memory usage
uint32_t free_heap = 65535;
uint32_t min_heap = 65535;
unsigned long next_heap_check = 0;

bool status_done = false;
bool web_setup_done = false;
unsigned long status_timeout;

void setup()
{
    Serial.begin(115200);
    LittleFS.begin(true);
    Serial.printf("\n\n\n=== R A T G D O ===\n");

    load_all_config_settings();
    userConfig->set(cfg_softAPmode, false);
    if (!userConfig->getSoftAPmode())
    {
        // Don't try and talk to garage door if booting in soft AP mode
        setup_comms();
    }
    setup_homekit();
}

void loop()
{
    comms_loop();
    if (web_setup_done)
        web_loop();
}
