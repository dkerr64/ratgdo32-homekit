/****************************************************************************
 * RATGDO HomeKit for ESP32
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-24 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 */
#pragma once

// C/C++ language includes
#include <stdint.h>

// Arduino includes
#include <Ticker.h>

// RATGDO project includes
// none

#define FLASH_MS 500 // default flash period, 500ms

class LED
{
private:
    uint8_t activeState = 0;     // 0 == LED on, 1 == LED off
    uint8_t idleState = 1;       // opposite of active
    unsigned long resetTime = 0; // Stores time when LED should return to idle state
    Ticker LEDtimer;
    static LED *instancePtr;
    LED();

public:
    LED(const LED &obj) = delete;
    static LED *getInstance() { return instancePtr; }

    void on();
    void off();
    void idle();
    void flash(unsigned long ms = 0);
    void setIdleState(uint8_t state);
    uint8_t getIdleState() { return idleState; };
};

extern LED *led;
