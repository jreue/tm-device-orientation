#include "Timer.h"

void Timer::drawCircularTimer(Adafruit_SSD1306& oled, unsigned long startTime,
                              unsigned long durationMs) {
  const int segments = 60;  // Number of segments for smoothness
  unsigned long now = millis();
  float progress = (float)(now - startTime) / durationMs;
  if (progress > 1.0f)
    progress = 1.0f;
  float angle = progress * 360.0f;
  oled.clearDisplay();
  // Draw filled circle (full timer)
  oled.fillCircle(timerX, timerY, timerRadius, SSD1306_WHITE);
  // Erase segments from 0 to angle (clockwise from 12 o'clock)
  for (float a = 0; a < angle; a += (360.0f / segments)) {
    float theta1 = (a - 90) * 3.1415926f / 180.0f;
    float theta2 = (a + (360.0f / segments) - 90) * 3.1415926f / 180.0f;
    int x0 = timerX;
    int y0 = timerY;
    int eraseRadius = timerRadius + 2;
    int x1 = timerX + (int)(eraseRadius * cos(theta1));
    int y1 = timerY + (int)(eraseRadius * sin(theta1));
    int x2 = timerX + (int)(eraseRadius * cos(theta2));
    int y2 = timerY + (int)(eraseRadius * sin(theta2));
    oled.fillTriangle(x0, y0, x1, y1, x2, y2, SSD1306_BLACK);
  }
  oled.display();
}

void Timer::drawHorizontalTimer(Adafruit_SSD1306& oled, unsigned long startTime,
                                unsigned long durationMs) {
  unsigned long now = millis();
  float progress = (float)(now - startTime) / durationMs;
  if (progress > 1.0f)
    progress = 1.0f;
  int barHeight = 4;  // Height of the timer bar
  int barWidth = OLED_SCREEN_WIDTH;
  int filledWidth = (int)((1.0f - progress) * barWidth);
  int barY = OLED_SCREEN_HEIGHT - barHeight;
  // Draw background bar
  oled.fillRect(0, barY, barWidth, barHeight, SSD1306_BLACK);
  // Draw filled portion
  if (filledWidth > 0) {
    oled.fillRect(0, barY, filledWidth, barHeight, SSD1306_WHITE);
  }
  oled.display();
}