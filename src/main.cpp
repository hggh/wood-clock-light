#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <time.h>

#include <FastLED.h>
#include <Bounce2.h>
#include <FS.h>
#include <WiFiManager.h>
#include <WebServer.h>

#include "display.h"

// we have one sacrificial LED with a diode on VCC to push VDD
#define LED_COUNT 86
#define PIN_RGB_LEDS 22
#define PIN_WIFI_RESET 17
#define HOSTNAME "woodie"

#define COLOR_MODE_RAINBOW 1
#define COLOR_MODE_SINGLE_COLOR 2

CRGB leds[LED_COUNT];
CRGB led_color = CRGB::Red;
WebServer server(80);
WiFiManager wm;
Bounce2::Button wifi_cfg_reset = Bounce2::Button();

const long gmt_offset_sec = 3600;
const int  day_light_offset_sec = 3600;

uint8_t time_hour = 255;
uint8_t time_minute = 255;
uint8_t color_mode = COLOR_MODE_RAINBOW;
int led_brightness = 60;
String html_color_code = "#ff0000";
bool update_leds = false;
CHSV hsv;

void write_char_to_leds(uint8_t offset, const uint8_t n[15]) {
  for (uint8_t i = 0; i<15; i++) {
    if (n[i] == 1) {
      if (color_mode == COLOR_MODE_RAINBOW) {
        leds[offset + i] = hsv;
        hsv.hue += 10;
      }
      else {
        leds[offset + i] = led_color;
      }
    }
    else {
      leds[offset + i] = CRGB::Black;
    }
  }
}

void clock_display_number(uint8_t offset, uint8_t number, boolean display_zero = false) {
  uint8_t t_first = ((number / 10 ) % 10);
  uint8_t t_second = (number % 10 );

  if (display_zero == true || t_first != 0 ) {
    write_char_to_leds(offset, numbers[t_first]);
  }
  write_char_to_leds(offset + 20, numbers[t_second]);
}

void webHandleRoot() {
  String content = "";
  File file = SPIFFS.open("/index.html");
  while(file.available()){
    content += char(file.read());
  }
  content.replace("LED_BRIGHTNESS_VALUE", String(led_brightness));
  content.replace("LED_SINGLE_COLOR_CODE", html_color_code);
  content.replace("LED_COLOR_MODE_SETTING", String(color_mode));
  file.close();
  server.send(200, "text/html", content);
}

void webHandleUpdate() {
  String req_mode = server.arg("mode");
  if (req_mode.equals("rainbow")) {
    color_mode = COLOR_MODE_RAINBOW;
  }
  else {
    String req_color_single = server.arg("color");
    html_color_code = req_color_single;
    req_color_single.replace("#", "");

    long color_code = strtol(req_color_single.c_str(), NULL, 16);
    led_color = CRGB(color_code);

    color_mode = COLOR_MODE_SINGLE_COLOR;
  }

  String req_brightness = server.arg("brightness");
  led_brightness = req_brightness.toInt();
  FastLED.setBrightness(led_brightness);

  update_leds = true;
  webHandleRoot();
}

void wm_config_mode_callback(WiFiManager *myWiFiManager) {
  Serial.println("Entering WiFI Manager configuration....");

  fill_solid(leds, LED_COUNT, CRGB::DeepPink);
  FastLED.setBrightness(10);
  FastLED.show();
}

void setup() {
  btStop();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  Serial.begin(115200);

  hsv.hue = 1;
  hsv.val = 255;
  hsv.sat = 240;

  FastLED.addLeds<PL9823, PIN_RGB_LEDS>(leds, LED_COUNT);
  FastLED.setBrightness(led_brightness);

  if(!SPIFFS.begin(true)){
    Serial.println("error on SPIFFS...");
  }
  wm.setAPCallback(wm_config_mode_callback);
  wm.autoConnect(HOSTNAME, "");

  FastLED.clear(true);

  server.on("/", webHandleRoot);
  server.on("/update", webHandleUpdate);
  server.begin();

  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  configTime(gmt_offset_sec, day_light_offset_sec, "0.de.pool.ntp.org", "1.de.pool.ntp.org", "2.de.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  wifi_cfg_reset.attach(PIN_WIFI_RESET, INPUT_PULLUP);
  wifi_cfg_reset.interval(25);
  wifi_cfg_reset.setPressedState(LOW);
}

void loop() {
  wifi_cfg_reset.update();
  server.handleClient();

  if (wifi_cfg_reset.pressed()) {
    // WiFi reset configuration button was pressed
    wm.resetSettings();
    Serial.println("WiFi reset configuration button was pressed......");
    Serial.flush();
    delay(500);
    ESP.restart();
  }

  struct tm timedate;
  getLocalTime(&timedate);

  if (timedate.tm_min != time_minute || timedate.tm_hour != time_hour || update_leds == true) {
    // time has changed, display it on the WS2812b LEDs
    update_leds = false;
    time_minute = timedate.tm_min;
    time_hour = timedate.tm_hour;

    Serial.println("Time has changed...");
    Serial.print("Hour: ");
    Serial.println(time_hour);
    Serial.print("Minute: ");
    Serial.println(time_minute);

    if (time_hour == 0) {
      clock_display_number(0 + 1, time_hour, true);
    }
    else {
      clock_display_number(0 + 1, time_hour, false);
    }
    write_char_to_leds(35 + 1, time_delimiter);
    clock_display_number(50 + 1, time_minute, true);

    FastLED.show();
  }
  delay(1);
}
