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

// ESP system includes

// RATGDO project includes
#include "ratgdo.h"
#include "config.h"
#include "comms.h"
#include "utilities.h"
#include "homekit.h"
#include "web.h"
#include "softAP.h"

// Logger tag
static const char *TAG = "ratgdo-homekit";

static DEV_GarageDoor *door;
static DEV_Light *light;
static DEV_Motion *motion;

static bool isPaired = false;
static bool homekit_setup_done = false;

void wifiCallbackAll(int count)
{
    RINFO(TAG, "WiFi established, IP: %s, count: %d", WiFi.localIP().toString().c_str(), count);
    userConfig->set(cfg_localIP, WiFi.localIP().toString().c_str());
    userConfig->set(cfg_gatewayIP, WiFi.gatewayIP().toString().c_str());
    userConfig->set(cfg_subnetMask, WiFi.gatewayIP().toString().c_str());
    userConfig->set(cfg_nameserverIP, WiFi.dnsIP().toString().c_str());
    userConfig->save();
    RINFO(TAG, "WiFi Got IP: %s, Mask: %s, Gateway: %s, DNS: %s", userConfig->getLocalIP().c_str(),
          userConfig->getSubnetMask().c_str(), userConfig->getGatewayIP().c_str(), userConfig->getNameserverIP().c_str());
    if (!softAPmode)
    {
        if (userConfig->getTimeZone() == "")
        {
            get_auto_timezone();
        }
        setup_comms();
        setup_web();
    }
    else
    {
        RINFO(TAG, "in SoftAP mode, do not initialize services");
    }
}

void statusCallback(HS_STATUS status)
{
    switch (status)
    {
    case HS_WIFI_NEEDED:
        RINFO(TAG, "Status: No WiFi Credentials, need to provision");
        break;
    case HS_WIFI_CONNECTING:
        RINFO(TAG, "Status: WiFi connecting, set hostname: %s", device_name_rfc952);
        WiFi.setSleep(WIFI_PS_NONE);
        WiFi.hostname((const char *)device_name_rfc952);
        break;
    case HS_PAIRING_NEEDED:
        RINFO(TAG, "Status: Need to pair");
        isPaired = false;
        break;
    case HS_PAIRED:
        RINFO(TAG, "Status: Paired");
        isPaired = true;
        break;
    case HS_REBOOTING:
        RINFO(TAG, "Status: Rebooting");
        break;
    case HS_FACTORY_RESET:
        RINFO(TAG, "Status: Factory Reset");
    default:
        RINFO(TAG, "HomeSpan Status: %s", homeSpan.statusString(status));
        // uint8_t *p = Span::accessory.ID;
        break;
    }
}

void setup_homekit()
{
    homeSpan.setLogLevel(0);
    homeSpan.setSketchVersion(AUTO_VERSION);
    homeSpan.setHostNameSuffix("");
    homeSpan.setPortNum(5556);
    homeSpan.setStatusPin(LED_BUILTIN);

    homeSpan.enableAutoStartAP();
    homeSpan.setApFunction(start_soft_ap);

    homeSpan.setQRID("RTGO");
    homeSpan.setPairingCode("25102023"); // On Oct 25, 2023, Chamberlain announced they were disabling API
                                         // access for "unauthorized" third parties.

    homeSpan.setWifiCallbackAll(wifiCallbackAll);
    homeSpan.setStatusCallback(statusCallback);

    homeSpan.begin(Category::GarageDoorOpeners, device_name, device_name_rfc952, "ratgdo-ESP32");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("Garage Door");
    new Characteristic::Manufacturer("Ratcloud llc");
    new Characteristic::SerialNumber("123-ABC");
    new Characteristic::Model("ratgdo-ESP32");
    new Characteristic::FirmwareRevision(AUTO_VERSION);

    door = new DEV_GarageDoor();
    light = new DEV_Light();
    motion = new DEV_Motion();

    homeSpan.autoPoll(8192, 1, 1);
    homekit_setup_done = true;
}

void queueSendHelper(QueueHandle_t q, GDOEvent e, const char *txt)
{
    if (!q || xQueueSend(q, &e, 0) == errQUEUE_FULL)
    {
        RERROR(TAG, "Could not queue homekit notify of %s state: %d", e.value.u);
    }
}

void homekit_unpair()
{
    if (!homekit_setup_done && !isPaired)
        return;

    homeSpan.processSerialCommand("U");
}

bool homekit_is_paired()
{
    return isPaired;
}

/****************************************************************************
 * Garage Door Service Handler
 */
void notify_homekit_target_door_state_change()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.c = door->target;
    e.value.u = garage_door.target_state;
    queueSendHelper(door->event_q, e, "target door");
}

void notify_homekit_current_door_state_change()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.c = door->current;
    e.value.u = garage_door.current_state;
    queueSendHelper(door->event_q, e, "current door");
}

void notify_homekit_target_lock()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.c = door->lockTarget;
    e.value.u = garage_door.target_lock;
    queueSendHelper(door->event_q, e, "target lock");
}

void notify_homekit_current_lock()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.c = door->lockCurrent;
    e.value.u = garage_door.current_lock;
    queueSendHelper(door->event_q, e, "current lock");
}

void notify_homekit_obstruction()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.c = door->obstruction;
    e.value.b = garage_door.obstructed;
    queueSendHelper(door->event_q, e, "obstruction");
}

DEV_GarageDoor::DEV_GarageDoor()
{
    RINFO(TAG, "Configuring HomeKit Garage Door Service");
    event_q = xQueueCreate(5, sizeof(GDOEvent));
    current = new Characteristic::CurrentDoorState(current->CLOSED);
    target = new Characteristic::TargetDoorState(target->CLOSED);
    obstruction = new Characteristic::ObstructionDetected(obstruction->NOT_DETECTED);
    lockCurrent = new Characteristic::LockCurrentState(lockCurrent->UNKNOWN);
    lockTarget = new Characteristic::LockTargetState(lockTarget->UNLOCK);
}

boolean DEV_GarageDoor::update()
{
    if (target->getNewVal() == target->OPEN)
    {
        RINFO(TAG, "Opening Garage Door");
        current->setVal(current->OPENING);
        obstruction->setVal(false);
        open_door();
    }
    else
    {
        RINFO(TAG, "Closing Garage Door");
        current->setVal(current->CLOSING);
        obstruction->setVal(false);
        close_door();
    }

    if (lockTarget->getNewVal() == lockTarget->LOCK)
    {
        RINFO(TAG, "Locking Garage Door Remotes");
        set_lock(lockTarget->LOCK);
    }
    else
    {
        RINFO(TAG, "Unlocking Garage Door Remotes");
        set_lock(lockTarget->UNLOCK);
    }

    return true;
}

void DEV_GarageDoor::loop()
{
    if (uxQueueMessagesWaiting(event_q) > 0)
    {
        GDOEvent e;
        xQueueReceive(event_q, &e, 0);
        RINFO(TAG, "Garage Door set value: %d", e.value.u);
        e.c->setVal(e.value.u);
    }
    /*
        if (current->getVal() == target->getVal()) // if current-state matches target-state there is nothing do -- exit loop()
            return;

        if (current->getVal() == current->CLOSING && random(100000) == 0)
        {                                      // here we simulate a random obstruction, but only if the door is closing (not opening)
            current->setVal(current->STOPPED); // if our simulated obstruction is triggered, set the curent-state to STOPPED
            obstruction->setVal(true);         // and set obstruction-detected to true
            LOG1("Garage Door Obstruction Detected!\n");
        }

        if (current->getVal() == current->STOPPED) // if the current-state is stopped, there is nothing more to do - exit loop()
            return;

        // This last bit of code only gets called if the door is in a state that represents actively opening or actively closing.
        // If there is an obstruction, the door is "stopped" and won't start again until the HomeKit Controller requests a new open or close action

        if (target->timeVal() > 5000)          // simulate a garage door that takes 5 seconds to operate by monitoring time since target-state was last modified
            current->setVal(target->getVal()); // set the current-state to the target-state
            */
}

/****************************************************************************
 * Light Service Handler
 */
void notify_homekit_light()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.value.b = garage_door.light;
    queueSendHelper(light->event_q, e, "light");
}

DEV_Light::DEV_Light()
{
    RINFO(TAG, "Configuring HomeKit Light Service");
    event_q = xQueueCreate(5, sizeof(GDOEvent));
    on = new Characteristic::On(on->OFF);
}

boolean DEV_Light::update()
{
    RINFO(TAG, "Turn light %s", on->getNewVal<bool>() ? "on" : "off");
    set_light(on->getNewVal<bool>());
    return true;
}

void DEV_Light::loop()
{
    if (uxQueueMessagesWaiting(event_q) > 0)
    {
        GDOEvent e;
        xQueueReceive(event_q, &e, 0);
        RINFO(TAG, "Light has turned %s", e.value.b ? "on" : "off");
        on->setVal(e.value.b);
    }
}

/****************************************************************************
 * Motion Service Handler
 */
void notify_homekit_motion()
{
    if (!homekit_setup_done && !isPaired)
        return;

    GDOEvent e;
    e.value.b = garage_door.motion;
    queueSendHelper(motion->event_q, e, "motion");
}

DEV_Motion::DEV_Motion()
{
    RINFO(TAG, "Configuring HomeKit Motion Service");
    event_q = xQueueCreate(5, sizeof(GDOEvent));
    motion = new Characteristic::MotionDetected(motion->NOT_DETECTED);
}

void DEV_Motion::loop()
{
    if (uxQueueMessagesWaiting(event_q) > 0)
    {
        GDOEvent e;
        xQueueReceive(event_q, &e, 0);
        RINFO(TAG, "Motion %s", e.value.b ? "detected" : "reset");
        motion->setVal(e.value.b);
    }
}
