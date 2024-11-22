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

// Arduino includes
#include <LittleFS.h>

// ESP system files
#include <Network.h>
#include <nvs_flash.h>
#include <nvs.h>

// RATGDO project includes
#include "ratgdo.h"
#include "config.h"
#include "utilities.h"
#include "comms.h"
#include "softAP.h"

// Logger tag
static const char *TAG = "ratgdo-config";

char default_device_name[DEVICE_NAME_SIZE] = "";
char device_name[DEVICE_NAME_SIZE] = "";
char device_name_rfc952[DEVICE_NAME_SIZE] = "";

// Construct the singleton tasks for user config and NVRAM access
userSettings *userSettings::instancePtr = new userSettings();
userSettings *userConfig = userSettings::getInstance();
nvRamClass *nvRamClass::instancePtr = new nvRamClass();
nvRamClass *nvRam = nvRamClass::getInstance();

bool setDeviceName(const std::string &key, const std::string &name, configSetting *action)
{
    // Check we have a legal device name...
    // xSemaphoreTake(mutex, portMAX_DELAY);
    // take semaphore because multiple functions on same global block.
    make_rfc952(device_name_rfc952, name.c_str(), sizeof(device_name_rfc952));
    if (strlen(device_name_rfc952) == 0)
    {
        // cannot have a empty device name, reset to default...
        strlcpy(device_name, default_device_name, sizeof(device_name));
        make_rfc952(device_name_rfc952, default_device_name, sizeof(device_name_rfc952));
        // userConfig->set(cfg_deviceName, default_device_name);
    }
    else
    {
        // device name okay, copy it to our global
        strlcpy(device_name, name.c_str(), sizeof(device_name));
        // userConfig->set(cfg_deviceName, device_name);
    }
    // xSemaphoreGive(mutex);
    return true;
}

bool helperWiFiPower(const std::string &key, const std::string &value, configSetting *action)
{
    // Only reboot if value has changed
    if (std::get<int>(action->value) != std::stoi(value))
    {
        RINFO(TAG, "Setting WiFi power to: %s", value);
        userConfig->set(key, value);
        action->reboot = true;
    }
    else
    {
        RINFO(TAG, "WiFi power unchanged at: %s", value);
        action->reboot = false;
    }
    return true;
}

bool helperWiFiPhyMode(const std::string &key, const std::string &value, configSetting *action)
{
    // Only reboot if value has changed
    if (std::get<int>(action->value) != std::stoi(value))
    {
        RINFO(TAG, "Setting WiFi mode to: %s", value);
        userConfig->set(key, value);
        action->reboot = true;
    }
    else
    {
        RINFO(TAG, "WiFi mode unchanged at: %s", value);
        action->reboot = false;
    }
    return true;
}

bool helperGDOSecurityType(const std::string &key, const std::string &value, configSetting *action)
{
    // Call fn to reset door
    userConfig->set(key, value);
    reset_door();
    return true;
}

bool helperLEDidle(const std::string &key, const std::string &value, configSetting *action)
{
    // TODO call fn to set LED object
    userConfig->set(key, value);
    return true;
}

bool helperMotionTriggers(const std::string &key, const std::string &value, configSetting *action)
{
    // TODO call fn to set reboot only if required
    userConfig->set(key, value);
    return true;
}

bool helperTimeZone(const std::string &key, const std::string &value, configSetting *action)
{
    // TODO call fn to set system time zone
    userConfig->set(key, value);
    return true;
}

bool helperSyslogEn(const std::string &key, const std::string &value, configSetting *action)
{
    userConfig->set(key, value);
    // these globals are set to optimize log message handling...
    strlcpy(syslogIP, userConfig->getSyslogIP().c_str(), sizeof(syslogIP));
    syslogPort = userConfig->getSyslogPort();
    syslogEn = userConfig->getSyslogEn();
    return true;
}

/****************************************************************************
 * User settings class
 */
userSettings::userSettings()
{
    uint8_t mac[6];
    Network.macAddress(mac);
    snprintf(default_device_name, sizeof(default_device_name), "Garage Door %02X%02X%02X", mac[3], mac[4], mac[5]);
    setDeviceName("", std::string(default_device_name), NULL);
    configFile = "/user_config";
    // key, {reboot, wifiChanged, value, fn to call}
    settings = {
        {cfg_deviceName, {false, false, default_device_name, setDeviceName}}, // call fn to set global
        {cfg_wifiSettingsChanged, {true, true, false, NULL}},
        {cfg_wifiPower, {true, true, 20, helperWiFiPower}},    // call fn to set reboot only if setting changed
        {cfg_wifiPhyMode, {true, true, 0, helperWiFiPhyMode}}, // call fn to set reboot only if setting changed
        {cfg_staticIP, {true, true, false, NULL}},
        {cfg_localIP, {true, true, "0.0.0.0", NULL}},
        {cfg_subnetMask, {true, true, "0.0.0.0", NULL}},
        {cfg_gatewayIP, {true, true, "0.0.0.0", NULL}},
        {cfg_nameserverIP, {true, true, "0.0.0.0", NULL}},
        {cfg_passwordRequired, {false, false, false, NULL}},
        {cfg_wwwUsername, {false, false, "admin", NULL}},
        //  Credentials are MD5 Hash... server.credentialHash(username, realm, "password");
        {cfg_wwwCredentials, {false, false, "10d3c00fa1e09696601ef113b99f8a87", NULL}},
        {cfg_GDOSecurityType, {true, false, 2, helperGDOSecurityType}}, // call fn to reset door
        {cfg_TTCseconds, {false, false, 0, NULL}},
        {cfg_rebootSeconds, {true, true, 0, NULL}},
        {cfg_LEDidle, {false, false, 0, helperLEDidle}},              // call fn to set LED object
        {cfg_motionTriggers, {true, false, 0, helperMotionTriggers}}, // call fn to set reboot only if required
        {cfg_enableNTP, {true, false, false, NULL}},
        {cfg_doorUpdateAt, {false, false, 0, NULL}},
        // Will contain string of region/city and POSIX code separated by semicolon...
        // For example... "America/New_York;EST5EDT,M3.2.0,M11.1.0"
        // Current maximum string length is known to be 60 chars (+ null terminator), see JavaScript console log.
        {cfg_timeZone, {false, false, "", helperTimeZone}},       // call fn to set system time zone
        {cfg_softAPmode, {true, false, false, NULL}},
        {cfg_syslogEn, {false, false, false, helperSyslogEn}},    // call fn to set globals
        {cfg_syslogIP, {false, false, "0.0.0.0", NULL}},
        {cfg_syslogPort, {false, false, 514, NULL}},
    };
    mutex = xSemaphoreCreateMutex(); // need to serialize set's
}

void userSettings::toStdOut()
{
    for (const auto &it : settings)
    {
        if (holds_alternative<std::string>(it.second.value))
        {
            Serial.printf("%s:\t%s\n", it.first.c_str(), std::get<std::string>(it.second.value).c_str());
        }
        else if (holds_alternative<int>(it.second.value))
        {
            Serial.printf("%s:\t%d\n", it.first.c_str(), std::get<int>(it.second.value));
        }
        else
        {
            Serial.printf("%s:\t%d\n", it.first.c_str(), std::get<bool>(it.second.value));
        }
    }
}

void userSettings::toFile(Print &file)
{
    for (const auto &it : settings)
    {
        if (holds_alternative<std::string>(it.second.value))
        {
            file.printf("%s;%s\n", it.first.c_str(), std::get<std::string>(it.second.value).c_str());
        }
        else if (holds_alternative<int>(it.second.value))
        {
            file.printf("%s;%d\n", it.first.c_str(), std::get<int>(it.second.value));
        }
        else
        {
            file.printf("%s;%d\n", it.first.c_str(), std::get<bool>(it.second.value));
        }
    }
}

void userSettings::save()
{
    RINFO(TAG, "Writing user configuration to file: %s", configFile.c_str());
    File file = LittleFS.open(configFile.c_str(), "w");
    if (!file)
    {
        ESP_LOGW(TAG, "File open error");
        return;
    }
    toFile(file);
    file.close();
}

bool userSettings::load()
{
    RINFO(TAG, "Read user configuration from file: %s", configFile.c_str());
    File file = LittleFS.open(configFile.c_str(), "r");
    if (!file)
    {
        ESP_LOGW(TAG, "File does not exist");
        return false;
    }

    String line;
    while (file.available())
    {
        line = file.readStringUntil('\n');
        const char *key = line.c_str();
        // RINFO(TAG, "Read line from %s file: %s", configFile.c_str(), key);
        char *value = (char *)key;
        while (*value && (*value != ';'))
            value++;
        if (*value)
            *value++ = 0;
        set(key, (std::string)value);
    }

    file.close();
    return true;
}

bool userSettings::erase()
{
    LittleFS.remove(configFile.c_str());
    return true;
}

bool userSettings::contains(const std::string &key)
{
    return (settings.count(key) > 0);
}

std::variant<bool, int, std::string> userSettings::get(const std::string &key)
{
    return settings[key].value;
}

configSetting userSettings::getDetail(const std::string &key)
{
    return settings[key];
}

bool userSettings::set(const std::string &key, const bool value)
{
    bool rc = false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (settings.count(key))
    {
        if (holds_alternative<bool>(settings[key].value))
        {
            settings[key].value = value;
            rc = true;
        }
    }
    xSemaphoreGive(mutex);
    return rc;
}

bool userSettings::set(const std::string &key, const int value)
{
    bool rc = false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (settings.count(key))
    {
        if (holds_alternative<int>(settings[key].value))
        {
            settings[key].value = value;
            rc = true;
        }
        else if (holds_alternative<bool>(settings[key].value))
        {
            settings[key].value = (value != 0);
            rc = true;
        }
    }
    xSemaphoreGive(mutex);
    return rc;
}

bool userSettings::set(const std::string &key, const std::string &value)
{
    bool rc = false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (settings.count(key))
    {
        if (holds_alternative<std::string>(settings[key].value))
        {
            settings[key].value = value;
            rc = true;
        }
        else if (holds_alternative<bool>(settings[key].value))
        {
            settings[key].value = (value == "true") || (atoi(value.c_str()) != 0);
            rc = true;
        }
        else if (holds_alternative<int>(settings[key].value))
        {
            settings[key].value = stoi(value);
            rc = true;
        }
    }
    xSemaphoreGive(mutex);
    return rc;
}

bool userSettings::set(const std::string &key, const char *value)
{
    return set(key, std::string(value));
}

/****************************************************************************
 * NVRAM class
 */
nvRamClass::nvRamClass()
{
    RINFO(TAG, "Constructor for NVRAM class");
    // Initialize non volatile ram
    // We use this sparingly, most settings are saved in file system initialized below.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err = nvs_open("ratgdo", NVS_READWRITE, &nvHandle);
    if (err != ESP_OK)
    {
        RERROR(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        nvHandle = 0;
    }
}

int32_t nvRamClass::read(const std::string &key, int32_t dflt)
{
    int32_t value = dflt;
    esp_err_t err = nvs_get_i32(nvHandle, key.c_str(), &value);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        RERROR(TAG, "NVRAM get error for: %s (%s)", key.c_str(), esp_err_to_name(err));
    }
    return value;
}

std::string nvRamClass::read(const std::string &key, const std::string &dflt)
{
    std::string value = dflt;
    size_t len;
    esp_err_t err = nvs_get_str(nvHandle, key.c_str(), NULL, &len);
    if (err == ESP_OK)
    {
        char *buf = (char *)malloc(len);
        if (nvs_get_str(nvHandle, key.c_str(), buf, &len) == ESP_OK)
        {
            value = buf;
        }
        free(buf);
    }
    else if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        RERROR(TAG, "NVRAM get error for: %s (%s)", key.c_str(), esp_err_to_name(err));
    }
    return value;
}

bool nvRamClass::write(const std::string &key, int32_t value)
{
    esp_err_t err = nvs_set_i32(nvHandle, key.c_str(), value);
    if (err != ESP_OK)
    {
        RERROR(TAG, "NVRAM set error for: %s (%s)", key.c_str(), esp_err_to_name(err));
        return false;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(nvHandle));
    return true;
}

bool nvRamClass::write(const std::string &key, const std::string &value)
{
    esp_err_t err = nvs_set_str(nvHandle, key.c_str(), value.c_str());
    if (err != ESP_OK)
    {
        RERROR(TAG, "NVRAM set error for: %s (%s)", key.c_str(), esp_err_to_name(err));
        return false;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(nvHandle));
    return true;
}

bool nvRamClass::erase(const std::string &key)
{
    esp_err_t err = nvs_erase_key(nvHandle, key.c_str());
    if (err != ESP_OK)
    {
        RERROR(TAG, "NVRAM erase error for: %s (%s)", key.c_str(), esp_err_to_name(err));
        return false;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(nvHandle));
    return true;
}
