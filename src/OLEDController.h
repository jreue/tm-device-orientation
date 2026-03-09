#pragma once

#include <Adafruit_SSD1306.h>

class OLEDController {
  public:
    static void renderBootScreen(Adafruit_SSD1306& oled, int countdownSeconds);

    static void renderOrientationLayout(Adafruit_SSD1306& oled);
    static void renderOrientationValues(Adafruit_SSD1306& oled, int x, int y, int z,
                                        bool doDisplay);

    static void renderOffsetsSetup(Adafruit_SSD1306& oled);

    static void renderTransmitStaged(Adafruit_SSD1306& oled);
    static void renderTransmitComplete(Adafruit_SSD1306& oled);

    static void renderPhaseStaged(Adafruit_SSD1306& oled, int currentPhase, int totalPhases);
    static void renderPhaseLoading(Adafruit_SSD1306& oled, int currentPhase, int countdownSeconds);

    static void renderMasterWaitScreen(Adafruit_SSD1306& oled);
    static void renderSlaveWaitScreen(Adafruit_SSD1306& oled);

    static void renderInvalidSubmissionScreen(Adafruit_SSD1306& oled, int countdownSeconds);

  private:
    static void renderOrientationChrome(Adafruit_SSD1306& oled);
    static void renderPhaseProgressIndicators(Adafruit_SSD1306& oled, int currentPhase,
                                              int totalPhases);
};