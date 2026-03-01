#include "OLEDController.h"

#include "hardware_config.h"

void OLEDController::renderOrientation(Adafruit_SSD1306& oled, int x, int y, int z) {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 10);
  oled.println("Roll: " + String(x));
  oled.setCursor(0, 25);
  oled.println("Pitch: " + String(y));
  oled.setCursor(0, 40);
  oled.println("Yaw: " + String(z));
  oled.display();
}

void OLEDController::renderCalibrationSetup(Adafruit_SSD1306& oled) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(10, 24);
  oled.print("< Tuning Offsets >");
  oled.setCursor(42, 40);
  oled.print("WAIT...");
  oled.display();
}

void OLEDController::renderCalibrationStaged(Adafruit_SSD1306& oled) {
  oled.clearDisplay();

  // Render "Completed" centered
  oled.setTextSize(2);
  int completedTextWidth =
      9 * 12;  // Approximate width: 12 pixels per character, "Completed" is 9 characters
  oled.setCursor((OLED_SCREEN_WIDTH - completedTextWidth) / 2, 10);
  oled.println("Completed");

  // Render "Submit Calibration" centered below
  oled.setTextSize(1);
  int submitTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Submit Calibration" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - submitTextWidth) / 2, 40);
  oled.println("Submit Calibration");

  oled.display();
}

void OLEDController::renderCalibrationComplete(Adafruit_SSD1306& oled) {
  oled.clearDisplay();

  // Render "Calibration Complete" centered
  oled.setTextSize(1);
  int completeTextWidth =
      20 * 6;  // Approximate width: 6 pixels per character, "Calibration Complete" is 20 characters
  oled.setCursor((OLED_SCREEN_WIDTH - completeTextWidth) / 2, 24);
  oled.println("Calibration Complete");

  oled.display();
}

void OLEDController::renderRoundStaged(Adafruit_SSD1306& oled) {
  oled.clearDisplay();

  // Render "Start Round" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      11 * 6;  // Approximate width: 6 pixels per character, "Start Round" is 11 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Start Round");

  oled.display();
}

void OLEDController::renderRoundLoading(Adafruit_SSD1306& oled, int currentRound,
                                        int countdownSeconds) {
  int roundNumber = currentRound + 1;  // Display rounds starting from 1 instead of 0

  oled.clearDisplay();

  // Center "Round X" text on the horizontal axis
  oled.setTextSize(2);  // Make the "Round X" text bigger
  int roundTextWidth =
      12 * 6;  // Approximate width: 6 pixels per character, "Round X" is 12 characters max
  oled.setCursor((OLED_SCREEN_WIDTH - roundTextWidth) / 2, 10);
  oled.printf("Round %d", roundNumber);

  for (int countdown = countdownSeconds; countdown > 0; --countdown) {
    oled.setTextSize(4);
    oled.setCursor((OLED_SCREEN_WIDTH - 24) / 2,
                   (OLED_SCREEN_HEIGHT - 32) / 2 + 10);  // Move the number down slightly
    oled.printf("%d", countdown);
    oled.display();
    delay(1000);

    oled.fillRect((OLED_SCREEN_WIDTH - 24) / 2, (OLED_SCREEN_HEIGHT - 32) / 2 + 10, 24, 32,
                  BLACK);  // Clear the number
  }

  oled.clearDisplay();
  oled.display();
}

void OLEDController::renderMasterWaitScreen(Adafruit_SSD1306& oled) {
  oled.clearDisplay();

  // Render "Waiting for Slaves" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Waiting for Slaves" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Waiting for Slaves");

  oled.display();
}

void OLEDController::renderSlaveWaitScreen(Adafruit_SSD1306& oled) {
  oled.clearDisplay();

  // Render "Waiting for Master" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Waiting for Master" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Waiting for Master");

  oled.display();
}