#pragma once

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "hardware_config.h"

class Timer {
  public:
    static void drawCircularTimer(Adafruit_SSD1306& oled, unsigned long startTime,
                                  unsigned long durationMs);
    static void drawHorizontalTimer(Adafruit_SSD1306& oled, unsigned long startTime,
                                    unsigned long durationMs);

  private:
    static constexpr unsigned char timerRadius = 15;
    static constexpr unsigned char margin = 4;
    static constexpr unsigned int timerX = SCREEN_WIDTH - timerRadius - margin;
    static constexpr unsigned int timerY = SCREEN_HEIGHT - timerRadius - margin;
};