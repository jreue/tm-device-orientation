#pragma once

#include <Adafruit_SSD1306.h>

class OLEDController {
  public:
    static void renderOrientation(Adafruit_SSD1306& oled, int x, int y, int z);

    static void renderCalibrationSetup(Adafruit_SSD1306& oled);
    static void renderCalibrationStaged(Adafruit_SSD1306& oled);
    static void renderCalibrationComplete(Adafruit_SSD1306& oled);
    
    static void renderRoundStaged(Adafruit_SSD1306& oled);
    static void renderRoundLoading(Adafruit_SSD1306& oled, int currentRound, int countdownSeconds);

    static void renderMasterWaitScreen(Adafruit_SSD1306& oled);
    static void renderSlaveWaitScreen(Adafruit_SSD1306& oled);
};