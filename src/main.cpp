#include <M5EPD.h>
#include <Wifi.h>
#include "SHT3X.h"
#include "WiFiInfo.h"

#define LGFX_M5PAPER
#include <LovyanGFX.hpp>
#include "myFont.h"
#include "misc.h"

constexpr float FONT_SIZE_XLARGE = 5.0;
constexpr float FONT_SIZE_LARGE = 3.0;
constexpr float FONT_SIZE_SMALL = 1.0;
constexpr uint_fast16_t M5PAPER_SIZE_LONG_SIDE = 960;
constexpr uint_fast16_t M5PAPER_SIZE_SHORT_SIDE = 540;

rtc_time_t time_ntp;
rtc_date_t date_ntp{4, 1, 1, 1970};

// M5Paper uses Wire (not Wire1) for internal components (including SHT30).
TwoWire &wire_portA = Wire;
SemaphoreHandle_t xMutex = nullptr;
SHT3X::SHT3X sht30(wire_portA);
LGFX gfx;

constexpr FixColonBaselineFont myFixedFont = {myFont::myFont_data};

struct State {
  bool display;
  int8_t min;
  float tmp;
  float hum;
};

State lastState = {0};

inline int syncNTPTimeJP(void)
{
  constexpr auto NTP_SERVER1 = "ntp.nict.jp";
  constexpr auto NTP_SERVER2 = "time.cloudflare.com";
  constexpr auto NTP_SERVER3 = "time.google.com";
  constexpr auto TIME_ZONE = "JST-9";

  auto datetime_setter = [](const tm &datetime) {
    rtc_time_t time{
        static_cast<int8_t>(datetime.tm_hour),
        static_cast<int8_t>(datetime.tm_min),
        static_cast<int8_t>(datetime.tm_sec)};
    rtc_date_t date{
        static_cast<int8_t>(datetime.tm_wday),
        static_cast<int8_t>(datetime.tm_mon + 1),
        static_cast<int8_t>(datetime.tm_mday),
        static_cast<int16_t>(datetime.tm_year + 1900)};

    M5.RTC.setDateTime(date, time);
    date_ntp = date;
    time_ntp = time;
  };

  return syncNTPTime(datetime_setter, TIME_ZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
}

void handleBtnPPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  lastState.display = false;
  prettyEpdRefresh(gfx);
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.startWrite();
  gfx.setCursor(0, 0);
  if (!syncNTPTimeJP())
  {
    gfx.println("Succeeded to sync time");
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
    {
      gfx.print("getLocalTime:");
      gfx.println(&timeInfo, "%Y/%m/%d %H:%M:%S");
    }
  }
  else
  {
    gfx.println("Failed to sync time");
  }

  rtc_date_t date;
  rtc_time_t time;

  // Get RTC
  M5.RTC.getDateTime(date, time);
  gfx.print("RTC         :");
  gfx.printf("%04d/%02d/%02d ", date.year, date.mon, date.day);
  gfx.printf("%02d:%02d:%02d", time.hour, time.min, time.sec);
  gfx.endWrite();

  delay(1000);

  gfx.setTextSize(FONT_SIZE_LARGE);
  xSemaphoreGive(xMutex);
}

inline void handleBtnRPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  lastState.display = false;
  prettyEpdRefresh(gfx);
  xSemaphoreGive(xMutex);
}

void handleBtnLPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  lastState.display = false;
  prettyEpdRefresh(gfx);
  gfx.setCursor(0, 0);
  gfx.setTextSize(FONT_SIZE_SMALL);
  gfx.print("Good bye..");
  gfx.waitDisplay();
  M5.disableEPDPower();
  M5.disableEXTPower();
  M5.disableMainPower();
  esp_deep_sleep_start();
  while (true)
    ;
  xSemaphoreGive(xMutex);
}

void handleButton(void *pvParameters)
{
  while (true)
  {
    delay(500);
    M5.update();
    if (M5.BtnP.isPressed())
    {
      handleBtnPPress();
    }
    else if (M5.BtnR.isPressed())
    {
      handleBtnRPress();
    }
    else if (M5.BtnL.isPressed())
    {
      handleBtnLPress();
    }
  }
}

void setup(void)
{
  constexpr uint_fast16_t WIFI_CONNECT_RETRY_MAX = 60; // 10 = 5s
  constexpr uint_fast16_t WAIT_ON_FAILURE = 2000;

  M5.begin(true, false, true, true, true, true);
  WiFi.begin(WiFiInfo::SSID, WiFiInfo::PASS);

  gfx.init();
  gfx.setEpdMode(epd_mode_t::epd_fast);
  gfx.setRotation(1);
  // gfx.setFont(&fonts::lgfxJapanGothic_40);
  gfx.setFont(&myFixedFont);
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.print("Connecting to Wi-Fi network");
  for (int cnt_retry = 0;
       cnt_retry < WIFI_CONNECT_RETRY_MAX && !WiFi.isConnected();
       cnt_retry++)
  {
    delay(500);
    gfx.print(".");
  }
  gfx.println("");
  if (WiFi.isConnected())
  {
    gfx.print("Local IP: ");
    gfx.println(WiFi.localIP());
  }
  else
  {
    gfx.println("Failed to connect to a Wi-Fi network");
    delay(WAIT_ON_FAILURE);
  }

  // built-in sht30
  if (!sht30.begin())
  {
    gfx.println("Failed to initialize external I2C");
  }

  xMutex = xSemaphoreCreateMutex();
  if (xMutex != nullptr)
  {
    xSemaphoreGive(xMutex);
    xTaskCreatePinnedToCore(handleButton, "handleButton", 4096, nullptr, 1, nullptr, 1);
  }
  else
  {
    gfx.println("Failed to create a task for buttons");
  }
  gfx.println("Init done");
  delay(1000);
  gfx.setTextSize(FONT_SIZE_LARGE);
  prettyEpdRefresh(gfx);
  gfx.setCursor(0, 0);
}

void loop(void)
{
  constexpr uint_fast16_t SLEEP_SEC = 10;
  constexpr uint_fast32_t TIME_SYNC_CYCLE = 3600 * 24 / SLEEP_SEC;

  static uint32_t cnt = TIME_SYNC_CYCLE - 3;

  xSemaphoreTake(xMutex, portMAX_DELAY);

  float tmp = 0.0;
  float hum = 0;

  int shterr = sht30.read();
  if (!shterr)
  {
    tmp = sht30.getTemperature();
    hum = sht30.getHumidity();
  }

  rtc_date_t date;
  rtc_time_t time;

  M5.RTC.getDateTime(date, time);

  if (
    !lastState.display
    || lastState.min != time.min
    || lastState.tmp != tmp
    || lastState.hum != hum
  ) {
    lastState.display = true;
    lastState.min = time.min;
    lastState.tmp = tmp;
    lastState.hum = hum;

    gfx.startWrite();
    gfx.fillScreen(TFT_WHITE);
    gfx.fillRect(0.57 * M5PAPER_SIZE_LONG_SIDE, 0, 3, M5PAPER_SIZE_SHORT_SIDE, TFT_BLACK);

    constexpr uint_fast16_t offset_y = 30;
    constexpr uint_fast16_t offset_x = 45;

    gfx.setCursor(0, offset_y);
    gfx.setClipRect(offset_x, offset_y, M5PAPER_SIZE_LONG_SIDE - offset_x, M5PAPER_SIZE_SHORT_SIDE - offset_y);
    gfx.setTextSize(FONT_SIZE_XLARGE);
    gfx.printf("%02d:%02d\r\n", time.hour, time.min);
    gfx.setTextSize(FONT_SIZE_LARGE);
    if (!shterr) {
      gfx.printf("%02.1f℃\r\n", tmp);
      gfx.printf("%02.1f%%\r\n", hum);
    } else {
      gfx.printf("--.-℃\r\n");
      gfx.printf("--.-%%\r\n");
      gfx.setTextSize(FONT_SIZE_SMALL);
      gfx.printf("SHT30 err: %d", shterr);
      gfx.setTextSize(FONT_SIZE_LARGE);
    }
    gfx.clearClipRect();

    constexpr float x = 0.61 * M5PAPER_SIZE_LONG_SIDE;
    gfx.setCursor(0, offset_y);
    gfx.setClipRect(x, offset_y, M5PAPER_SIZE_LONG_SIDE - offset_x - x, M5PAPER_SIZE_SHORT_SIDE - offset_y);
    gfx.printf("%04d\r\n", date.year);
    gfx.printf("%02d/%02d\r\n", date.mon, date.day);
    gfx.println(weekdayToString(date.week));
    gfx.clearClipRect();

    constexpr float offset_y_info = 0.75 * M5PAPER_SIZE_SHORT_SIDE;
    gfx.setCursor(0, offset_y_info);
    gfx.setTextSize(FONT_SIZE_SMALL);
    gfx.setClipRect(x, offset_y_info, M5PAPER_SIZE_LONG_SIDE - x, gfx.height() - offset_y_info);
    gfx.print("WiFi: ");
    gfx.println(WiFiConnectedToString());

    constexpr uint32_t low = 3300;
    constexpr uint32_t high = 4350;

    auto vol = std::min(std::max(M5.getBatteryVoltage(), low), high);
    // non-reliable battery gauge:
    // https://github.com/m5stack/M5Stack/issues/74#issuecomment-471381215
    // doesn't work.
    // (M2Paper uses SLM6635 for the battery manager and it doesn't support i2c)
    float percentage = static_cast<float>(vol - low) / static_cast<float>(high - low) * 100.0f;
    gfx.printf("BAT : %2.1f%%/%04dmv\r\n", percentage, vol);
    gfx.print("NTP : ");
    if (date_ntp.year == 1970)
    {
      gfx.print("YET"); // not initialized
    }
    else
    {
      gfx.printf("%02d/%02d %02d:%02d",
                date_ntp.mon, date_ntp.day,
                time_ntp.hour, time_ntp.min);
    }

    gfx.clearClipRect();
    gfx.setTextSize(FONT_SIZE_LARGE);
    gfx.endWrite();
  }

  cnt++;
  if (cnt >= TIME_SYNC_CYCLE)
  {
    syncNTPTimeJP();
    lastState.display = false;
    cnt = 0;
  }
  xSemaphoreGive(xMutex);
  if (60 - time.sec < SLEEP_SEC) {
    delay((60 - time.sec) * 1000);
  } else {
    delay(SLEEP_SEC * 1000);
  }
}
