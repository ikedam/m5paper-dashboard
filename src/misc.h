#pragma once

#include <functional>

#include <LovyanGFX.hpp>

inline String WiFiConnectedToString(void)
{
    return WiFi.isConnected() ? String("OK") : String("NG");
}

String weekdayToString(const int8_t weekDay)
{
    switch (weekDay)
    {
    case 0:
        return String("日");
    case 1:
        return String("月");
    case 2:
        return String("火");
    case 3:
        return String("水");
    case 4:
        return String("木");
    case 5:
        return String("金");
    case 6:
        return String("土");
    }
    return String("");
}

inline void prettyEpdRefresh(LGFX &gfx)
{
    gfx.setEpdMode(epd_mode_t::epd_quality);
    gfx.fillScreen(TFT_WHITE);
    gfx.setEpdMode(epd_mode_t::epd_fast);
}

int syncNTPTime(std::function<void(const tm &)> datetimeSetter, const char *tz,
                const char *server1, const char *server2 = nullptr, const char *server3 = nullptr)
{
    if (!WiFi.isConnected())
    {
        return 1;
    }

    configTzTime(tz, server1, server2, server3);

    // https://github.com/espressif/esp-idf/blob/master/examples/protocols/sntp/main/sntp_example_main.c
    int retry = 0;
    constexpr int retry_count = 50;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && ++retry < retry_count)
    {
        delay(100);
    }
    if (retry == retry_count)
    {
        return 1;
    }

    struct tm datetime;
    if (!getLocalTime(&datetime))
        return 1;

    datetimeSetter(datetime);

    return 0;
}