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


// Arduino includes
#include <Arduino.h>

// ESP system includes
#include <driver/gpio.h>

// RATGDO project includes
#include "HomeSpan.h"
#include "log.h"

#define DEVICE_NAME "homekit-ratgdo"
#define MANUF_NAME "ratCloud llc"
#define SERIAL_NUMBER "0P3ND00R"
#define MODEL_NAME "ratgdo_32"
#define CHIP_FAMILY "ESP32"

/********************************** PIN DEFINITIONS *****************************************/

const gpio_num_t UART_TX_PIN = GPIO_NUM_17;
const gpio_num_t UART_RX_PIN = GPIO_NUM_21;
const gpio_num_t LED_BUILTIN = GPIO_NUM_2;

// TODO obstruction refactor
// const gpio_num_t INPUT_OBST_PIN = GPIO_NUM_13; // black obstruction sensor terminal

/*
// TODO add support for dry contact switches
#define STATUS_DOOR_PIN         D0  // output door status, HIGH for open, LOW for closed
#define STATUS_OBST_PIN D8 // output for obstruction status, HIGH for obstructed, LOW for clear
#define DRY_CONTACT_OPEN_PIN    D5  // dry contact for opening door
#define DRY_CONTACT_CLOSE_PIN   D6  // dry contact for closing door
#define DRY_CONTACT_LIGHT_PIN   D3  // dry contact for triggering light (no discrete light commands, so toggle only)
*/

extern uint32_t free_heap;
extern uint32_t min_heap;
extern bool web_setup_done;

/********************************** MODEL *****************************************/

enum GarageDoorCurrentState : uint8_t
{
    CURR_OPEN = Characteristic::CurrentDoorState::OPEN,
    CURR_CLOSED = Characteristic::CurrentDoorState::CLOSED,
    CURR_OPENING = Characteristic::CurrentDoorState::OPENING,
    CURR_CLOSING = Characteristic::CurrentDoorState::CLOSING,
    CURR_STOPPED = Characteristic::CurrentDoorState::STOPPED,
};

enum GarageDoorTargetState : uint8_t
{
    TGT_OPEN = Characteristic::TargetDoorState::OPEN,
    TGT_CLOSED = Characteristic::TargetDoorState::CLOSED,
};

enum LockCurrentState : uint8_t
{
    CURR_UNLOCKED = Characteristic::LockCurrentState::UNLOCKED,
    CURR_LOCKED = Characteristic::LockCurrentState::LOCKED,
    CURR_JAMMED = Characteristic::LockCurrentState::JAMMED,
    CURR_UNKNOWN = Characteristic::LockCurrentState::UNKNOWN,
};

enum LockTargetState : uint8_t
{
    TGT_UNLOCKED = Characteristic::LockTargetState::UNLOCK,
    TGT_LOCKED = Characteristic::LockTargetState::LOCK,
};

struct GarageDoor
{
    bool active;
    GarageDoorCurrentState current_state;
    GarageDoorTargetState target_state;
    bool obstructed;
    bool has_motion_sensor;
    unsigned long motion_timer;
    bool motion;
    bool light;
    LockCurrentState current_lock;
    LockTargetState target_lock;
};

extern GarageDoor garage_door;

struct ForceRecover
{
   uint8_t push_count;
   unsigned long timeout;
};
