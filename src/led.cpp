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

// C/C++ language includes
// none

// RATGDO project includes
#include "ratgdo.h"
#include "led.h"

// Logger tag
static const char *TAG = "ratgdo-led";

// Construct the singleton object for LED access
LED *LED::instancePtr = new LED();
LED *led = LED::getInstance();

void LEDtimerCallback(LED *led)
{
    led->idle();
}

// Constructor for LED class
LED::LED()
{
    resetTime = 500;
    pinMode(LED_BUILTIN, OUTPUT);
}

void LED::on()
{
    digitalWrite(LED_BUILTIN, 0);
}

void LED::off()
{
    digitalWrite(LED_BUILTIN, 1);
}

void LED::idle()
{
    digitalWrite(LED_BUILTIN, idleState);
}

void LED::setIdleState(uint8_t state)
{
    // 0 = LED flashes off (idle is on)
    // 1 = LED flashes on (idle is off)
    // 3 = LED disabled (active and idle both off)
    if (state == 2)
    {
        idleState = activeState = 1;
    }
    else
    {
        idleState = state;
        activeState = (state == 1) ? 0 : 1;
    }
}

void LED::flash(unsigned long ms)
{
    digitalWrite(LED_BUILTIN, activeState);
    if (ms > 0 && ms != resetTime)
    {
        resetTime = ms;
        LEDtimer.once_ms(ms, LEDtimerCallback, this);
    }

}
