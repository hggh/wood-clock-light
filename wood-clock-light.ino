
#include <FastLED.h>
#include <Time.h>
#include <Wire.h>
#include <DCF77.h>
#include <DS1307RTC.h>

#include "display.h"
#include "config.h"


/*
 * Global Variables BEGIN
 */
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT);
CHSV hsv;

CRGB leds[LED_COUNT];

/*
 * set LED Brightness
 */
void set_led_brightness(uint8_t value) {
  FastLED.setBrightness(value);
}


/*
 * setup()
 */
void setup() {
  if (DEBUG) {
    Serial.begin(9600);
    Serial.println(F("Starting Wood Clock...."));
  }
  delay(200);
  if (!RTC.chipPresent() && DEBUG) {
    Serial.println("RTC Chip DS1307 not found on I2c");
  }

  DCF.Start();

  FastLED.addLeds<PL9823, LED_PIN>(leds, LED_COUNT);
  FastLED.clear(true);

  hsv.hue = 1;
  hsv.val = 255;
  hsv.sat = 240;

  while(true) {
    time_t DCFtime = DCF.getTime();
    if (DCFtime!=0) {
      if (DEBUG) {
        Serial.println("Time is updated");
      }
      setTime(DCFtime);
      RTC.set(DCFtime);
      break;
    }
    delay(500);
    if (DEBUG) {
      Serial.println("Waiting for Time from DCF77");
    }
  }
  set_led_brightness(30);
}


void write_char_to_leds(uint8_t offset, const uint8_t n[15]) {
  for (uint8_t i = 0; i<15; i++) {
    if (n[i] == 1) {
      leds[offset + i] = hsv;
      hsv.hue += 10;
    }
    else {
      leds[offset + i] = CRGB::Black;
    }
  }
}

/*
 * 
 */
void clock_display_number(uint8_t offset, uint8_t number, boolean display_zero = false) {

  uint8_t t_first = ((number / 10 ) % 10);
  uint8_t t_second = (number % 10 );

  if (display_zero == true || t_first != 0 ) {
    write_char_to_leds(offset, numbers[t_first]);
  }
  write_char_to_leds(offset + 20, numbers[t_second]);
}


/*
 * loop()
 */
uint8_t d_minute = 0;
uint8_t d_hour = 0;

void loop() {
  tmElements_t tm;
  RTC.read(tm);

  if (d_minute != tm.Minute || d_hour != tm.Hour) {
    d_hour = tm.Hour;
    d_minute = tm.Minute;
    if (DEBUG) {
      Serial.println("Setting Time on LEDs...");
      Serial.print("Uhrzeit: ");
      Serial.print(d_hour);
      Serial.print(":");
      Serial.println(d_minute);
    }
  
    if (d_hour == 0) {
      clock_display_number(0, d_hour, true);
    }
    else {
      clock_display_number(0, d_hour, false);
    }
    write_char_to_leds(35, time_delimiter);
    clock_display_number(50, d_minute, true);
    FastLED.show();
  }

  delay(200);
}
