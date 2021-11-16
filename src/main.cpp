#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include <FastLED.h>
#include <Bounce2.h>
#include <FS.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <MQTT.h>

#include "display.h"
#include "config.h"

// we have one sacrificial LED with a diode on VCC to push VDD
#define LED_COUNT 85
#define PIN_RGB_LEDS 17
#define PIN_WIFI_RESET 34

#define COLOR_MODE_RAINBOW 1
#define COLOR_MODE_SINGLE_COLOR 2

CRGB leds[LED_COUNT];
CRGB led_color = CRGB::Red;
WiFiManager wm;
WiFiClientSecure net;
MQTTClient mqtt;
Bounce2::Button wifi_cfg_reset = Bounce2::Button();

const long gmt_offset_sec = 3600;
const int  day_light_offset_sec = 3600;

uint8_t time_hour = 255;
uint8_t time_minute = 255;
uint8_t color_mode = COLOR_MODE_RAINBOW;
int led_brightness = 60;
volatile bool auth_token = false;
uint8_t auth_token_number = 0;
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

void mqtt_message_received(MQTTClient *client, char topic[], char bytes[], int length) {
  if (String(topic).endsWith("/led_brightness")) {
    update_leds = true;
    led_brightness = String(bytes).toInt();
    FastLED.setBrightness(led_brightness);
  }
  if (String(topic).endsWith("/auth_token")) {
    auth_token = true;
    auth_token_number = String(bytes).toInt();
  }
  if (String(topic).endsWith("/led_color")) {
    update_leds = true;

    String color_string = String(bytes);
    color_string.replace("#", "");
    long color_code_tmp = strtol(color_string.c_str(), NULL, 16);
    led_color = CRGB(color_code_tmp);
  }
  if (String(topic).endsWith("/color_mode")) {
    update_leds = true;

    if (String(bytes).equals("random_color")) {
      color_mode = COLOR_MODE_RAINBOW;
    }
    else {
      color_mode = COLOR_MODE_SINGLE_COLOR;
    }
  }
}

void connect_mqtt() {
  mqtt.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
  mqtt.subscribe(String("/device/" + String(HOSTNAME) + "/led_brightness").c_str(), 1);
  mqtt.subscribe(String("/device/" + String(HOSTNAME) + "/auth_token").c_str(), 1);
  mqtt.subscribe(String("/device/" + String(HOSTNAME) + "/color_mode").c_str(), 1);
  mqtt.subscribe(String("/device/" + String(HOSTNAME) + "/led_color").c_str(), 1);
  mqtt.subscribe(String("/device/" + String(HOSTNAME) + "/show_minute_leds").c_str(), 1);
  mqtt.onMessageAdvanced(mqtt_message_received);
  mqtt.publish(String("/device/" + String(HOSTNAME) + "/boot").c_str(), WiFi.localIP().toString().c_str(), false, 1);
}

void wm_config_mode_callback(WiFiManager *myWiFiManager) {
  Serial.println("Entering WiFI Manager configuration....");

  fill_solid(leds, LED_COUNT, CRGB::DeepPink);
  FastLED.setBrightness(10);
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
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

  configTime(gmt_offset_sec, day_light_offset_sec, "0.de.pool.ntp.org", "1.de.pool.ntp.org", "2.de.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  wifi_cfg_reset.attach(PIN_WIFI_RESET, INPUT);
  wifi_cfg_reset.interval(25);
  wifi_cfg_reset.setPressedState(LOW);

  net.setCACert(root_ca);
  mqtt.begin(MQTT_SERVER, MQTT_PORT, net);

}

void loop() {
  wifi_cfg_reset.update();
  if (mqtt.connected() == false) {
    connect_mqtt();
  }
  mqtt.loop();

  if (auth_token == true) {
    auth_token = false;
    FastLED.clear();
    clock_display_number(0, auth_token_number, false);
    FastLED.show();

    delay(5000);
    update_leds = true;
  }

  if (wifi_cfg_reset.pressed()) {
    // WiFi reset configuration button was pressed
    wm.resetSettings();
    Serial.println("WiFi reset configuration button was pressed......");
    Serial.flush();
    delay(500);
    ESP.restart();
  }

  struct tm timedate;
  time_t now = time(&now);
  localtime_r(&now, &timedate);

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
      clock_display_number(0, time_hour, true);
    }
    else {
      clock_display_number(0, time_hour, false);
    }
    write_char_to_leds(35, time_delimiter);
    clock_display_number(50, time_minute, true);

    FastLED.show();
  }
  delay(1);
}
