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
#include <stdint.h>

// Arduino includes
#include <WiFiUdp.h>

// RATGDO project includes
#include "ratgdo.h"
#include "log.h"
#include "config.h"
#include "utilities.h"
#include "secplus2.h"
// #include "comms.h"
#include "web.h"

// Logger tag
static const char *TAG = "ratgdo-logger";

#ifndef UNIT_TEST
void print_packet(uint8_t *pkt)
{
    RINFO(TAG, "decoded packet: [%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X]",
          pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5], pkt[6], pkt[7], pkt[8], pkt[9],
          pkt[10], pkt[11], pkt[12], pkt[13], pkt[14], pkt[15], pkt[16], pkt[17], pkt[18]);
}
#else  // UNIT_TEST
void print_packet(uint8_t pkt[SECPLUS2_CODE_LEN]) {}
#endif // UNIT_TEST

#ifdef LOG_MSG_BUFFER
// Construct the singleton object for LED access
LOG *LOG::instancePtr = new LOG();
LOG *ratgdoLogger = LOG::getInstance();

void logToSyslog(char *message);
bool syslogEn = false;
uint16_t syslogPort = 514;
char syslogIP[16] = "";
WiFiUDP syslog;

// Constructor for LOG class
LOG::LOG()
{
    logMutex = xSemaphoreCreateMutex();
    msgBuffer = (logBuffer *)malloc(sizeof(logBuffer));
    // Fill the buffer with space chars... because if we crash and dump buffer before it fills
    // up, we want blank space not garbage! Nothing is null-terminated in this circular buffer.
    memset(msgBuffer->buffer, 0x20, sizeof(msgBuffer->buffer));
    msgBuffer->wrapped = 0;
    msgBuffer->head = 0;
    lineBuffer = (char *)malloc(LINE_BUFFER_SIZE);
}

void LOG::logToBuffer(const char *fmt, ...)
{
    if (!lineBuffer)
    {
        static char buf[LINE_BUFFER_SIZE];
        // parse the format string into lineBuffer
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, LINE_BUFFER_SIZE, fmt, args);
        va_end(args);
        // print line to the serial port
        Serial.print(buf);
        return;
    }

    xSemaphoreTake(logMutex, portMAX_DELAY);
    // parse the format string into lineBuffer
    va_list args;
    va_start(args, fmt);
    vsnprintf(lineBuffer, LINE_BUFFER_SIZE, fmt, args);
    va_end(args);
    // print line to the serial port
    Serial.print(lineBuffer);

    // copy the line into the message save buffer
    size_t len = strlen(lineBuffer);
    size_t available = sizeof(msgBuffer->buffer) - msgBuffer->head;
    memcpy(&msgBuffer->buffer[msgBuffer->head], lineBuffer, min(available, len));
    if (available < len)
    {
        // we wrapped on the available buffer space
        msgBuffer->wrapped = 1;
        msgBuffer->head = len - available;
        memcpy(msgBuffer->buffer, &lineBuffer[available], msgBuffer->head);
    }
    else
    {
        msgBuffer->head += len;
    }
    // send it to subscribed browsers
    SSEBroadcastState(lineBuffer, LOG_MESSAGE);
    logToSyslog(lineBuffer);
    // Open the log message file as soon as we can so it is available for writing during a crash
    // This does not work in constructor because LittleFS not yet mounted.
    if (!logMessageFile)
        logMessageFile = (LittleFS.exists(CRASH_LOG_MSG_FILE)) ? LittleFS.open(CRASH_LOG_MSG_FILE, "r+") : LittleFS.open(CRASH_LOG_MSG_FILE, "w+");

    xSemaphoreGive(logMutex);
    return;
}

void LOG::printSavedLog(File file, Print &outputDev)
{
    if (file && file.size() > 0)
    {
        int num = LINE_BUFFER_SIZE;
        file.seek(0, fs::SeekSet);
        while (num == LINE_BUFFER_SIZE)
        {
            num = file.read((uint8_t *)lineBuffer, LINE_BUFFER_SIZE);
            outputDev.write(lineBuffer, num);
        }
        outputDev.println();
    }
}

void LOG::printSavedLog(Print &outputDev)
{
    if (logMessageFile)
        printSavedLog(logMessageFile, outputDev);
}

#ifdef ESP8266
// These are defined in the linker script, and filled in by the elf2bin.py util
extern "C" uint32_t __crc_len;
extern "C" uint32_t __crc_val;
#endif

void LOG::printMessageLog(Print &outputDev)
{
#ifdef NTP_CLIENT
    if (enableNTP && clockSet)
    {
        outputDev.write("Server time (secs): ");
        outputDev.println(time(NULL));
    }
#endif
    outputDev.write("Server uptime (ms): ");
    outputDev.println(millis());
    outputDev.write("Firmware version: ");
    outputDev.write(AUTO_VERSION);
    outputDev.println();
#ifdef ESP8266
    outputDev.write("Flash CRC: 0x");
    outputDev.println(__crc_val, 16);
    outputDev.write("Flash length: ");
    outputDev.println(__crc_len);
#endif
    outputDev.write("Free heap: ");
    outputDev.println(free_heap);
    outputDev.write("Minimum heap: ");
    outputDev.println(min_heap);
    outputDev.println();
    if (msgBuffer)
    {
        if (msgBuffer->wrapped != 0)
        {
            outputDev.write(&msgBuffer->buffer[msgBuffer->head], sizeof(msgBuffer->buffer) - msgBuffer->head);
        }
        outputDev.write(msgBuffer->buffer, msgBuffer->head);
    }
}

#define SYSLOG_LOCAL0 16
#define SYSLOG_EMERGENCY 0
#define SYSLOG_ALERT 1
#define SYSLOG_CRIT 2
#define SYSLOG_ERROR 3
#define SYSLOG_WARN 4
#define SYSLOG_NOTICE 5
#define SYSLOG_INFO 6
#define SYSLOG_DEBUG 7
#define SYSLOG_NIL "-"
#define SYSLOG_BOM "\xEF\xBB\xBF"

void logToSyslog(char *message)
{
    if (!syslogEn || !WiFi.isConnected())
        return;

    uint8_t PRI = SYSLOG_LOCAL0 * 8;
    if (*message == '>')
        PRI += SYSLOG_INFO;
    else if (*message == '!')
        PRI += SYSLOG_ERROR;

    char *app_name;
    char *msg;

    app_name = strtok(message, "]");
    while (*app_name == ' ')
        app_name++;
    app_name = strtok(NULL, ":");
    while (*app_name == ' ')
        app_name++;
    msg = strtok(NULL, "\r\n");
    while (*msg == ' ')
        msg++;

    syslog.beginPacket(syslogIP, syslogPort);
    // Use RFC5424 Format
    syslog.printf("<%u>1 ", PRI); // PRI code
#if defined(NTP_CLIENT) && defined(USE_NTP_TIMESTAMP)
    syslog.print((enableNTP && clockSet) ? timeString(0, true) : SYSLOG_NIL);
#else
    syslog.print(SYSLOG_NIL); // Time - let the syslog server insert time
#endif
    syslog.print(" ");
    syslog.print(device_name_rfc952); // hostname
    syslog.print(" ");
    syslog.print(app_name);     // application name
    syslog.printf(" 0");        // process ID
    syslog.print(" " SYSLOG_NIL // message ID
                 " " SYSLOG_NIL // structured data
#ifdef USE_UTF8_BOM
                 " " SYSLOG_BOM); // BOM - indicates UTF-8 encoding
#else
                 " "); // No BOM
#endif
    syslog.print(msg); // message
    syslog.endPacket();
}

#ifdef ENABLE_CRASH_LOG
void crashCallback()
{
    if (ratgdoLogger->msgBuffer && ratgdoLogger->logMessageFile)
    {
        // ratgdoLogger->logMessageFile.truncate(0);
        ratgdoLogger->logMessageFile.seek(0, fs::SeekSet);
        ratgdoLogger->logMessageFile.println();
        ratgdoLogger->printMessageLog(ratgdoLogger->logMessageFile);
        ratgdoLogger->logMessageFile.close();
    }
    // We may not have enough memory to open the file and save the code
    // save_rolling_code();
}
#endif

#endif // LOG_MSG_BUFFER
