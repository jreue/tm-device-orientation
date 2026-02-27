#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <MPU6050_light.h>
#include <shared_hardware_config.h>
#include <stdint.h>

#include "Button.h"
#include "EspNowHelper.h"
#include "Timer.h"
#include "Wire.h"
#include "hardware_config.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MPU6050 mpu(Wire);

uint8_t hubAddress[] = HUB_MAC_ADDRESS;
EspNowHelper espNowHelper;

const unsigned long ORIENTATION_DISPLAY_INTERVAL_MS = 100;

void setupDisplay();
void renderCalibrationSetup();
void renderOrientation();
void setupMPU();
void onSubmitButtonPressDownCb(void* button_handle, void* usr_data);
void onMasterCalibrateButtonPressDownCb(void* button_handle, void* usr_data);

// Round target definitions
struct Orientation {
    int x;
    int y;
    int z;
};

const Orientation roundTargets[3] = {
    {0, 0, 10},    // Round 1
    {10, 10, 20},  // Round 2
    {20, 20, 0}    // Round 3
};

bool roundCompleted[3] = {false, false, false};

// Helper to check if orientation matches target (with tolerance)
bool orientationMatches(const Orientation& target, int x, int y, int z, int tolerance = 2) {
  return abs(target.x - x) <= tolerance && abs(target.y - y) <= tolerance &&
         abs(target.z - z) <= tolerance;
}

// Returns true if all rounds are completed
bool isCalibrated() {
  return roundCompleted[0] && roundCompleted[1] && roundCompleted[2];
}

// Tracks which round is currently active (0-based)
int currentRound = 0;

void setup() {
  Serial.begin(115200);

  espNowHelper.begin(DEVICE_ID);
  espNowHelper.addPeer(hubAddress);
  espNowHelper.sendModuleConnected(hubAddress);

  Wire.begin();

  pinMode(LED_ROUND_1_SUCCESS_PIN, OUTPUT);
  pinMode(LED_ROUND_2_SUCCESS_PIN, OUTPUT);
  pinMode(LED_ROUND_3_SUCCESS_PIN, OUTPUT);
  pinMode(LED_CALIBRATED_PIN, OUTPUT);

  digitalWrite(LED_ROUND_1_SUCCESS_PIN, LOW);
  digitalWrite(LED_ROUND_2_SUCCESS_PIN, LOW);
  digitalWrite(LED_ROUND_3_SUCCESS_PIN, LOW);
  digitalWrite(LED_CALIBRATED_PIN, LOW);

  delay(2000);

  setupDisplay();

  renderCalibrationSetup();

  delay(1000);

  setupMPU();

  delay(1000);

  Button* btn = new Button(SUBMIT_BUTTON_PIN, false);
  Button* masterCalibrateButton = new Button(CALIBRATE_BUTTON_PIN, false);

  btn->attachPressDownEventCb(&onSubmitButtonPressDownCb, NULL);
  masterCalibrateButton->attachPressDownEventCb(&onMasterCalibrateButtonPressDownCb, NULL);
}

void updateRoundLEDs() {
  digitalWrite(LED_ROUND_1_SUCCESS_PIN, roundCompleted[0] ? HIGH : LOW);
  digitalWrite(LED_ROUND_2_SUCCESS_PIN, roundCompleted[1] ? HIGH : LOW);
  digitalWrite(LED_ROUND_3_SUCCESS_PIN, roundCompleted[2] ? HIGH : LOW);
  digitalWrite(LED_CALIBRATED_PIN, isCalibrated() ? HIGH : LOW);
}

void loop() {
  mpu.update();
  renderOrientation();
  updateRoundLEDs();
}

void onSubmitButtonPressDownCb(void* button_handle, void* usr_data) {
  Serial.println("Submit button pressed down");
  if (currentRound >= 3) {
    Serial.println("All rounds already completed.");
    return;
  }
  // Read current orientation
  int x = (int)mpu.getAngleX() * -1;
  int y = (int)mpu.getAngleY();
  int z = (int)mpu.getAngleZ() * -1;
  Serial.printf("Current orientation: x=%d, y=%d, z=%d\n", x, y, z);
  const Orientation& target = roundTargets[currentRound];
  if (orientationMatches(target, x, y, z)) {
    roundCompleted[currentRound] = true;
    Serial.printf("Round %d complete!\n", currentRound + 1);
    currentRound++;
    updateRoundLEDs();
    if (isCalibrated()) {
      Serial.println("Calibration complete!");
      digitalWrite(LED_CALIBRATED_PIN, HIGH);
    }
  } else {
    Serial.printf("Round %d not matched. Try again.\n", currentRound + 1);
  }
}

void onMasterCalibrateButtonPressDownCb(void* button_handle, void* usr_data) {
  Serial.println("Master calibrate button pressed down");
  if (isCalibrated()) {
    digitalWrite(LED_CALIBRATED_PIN, HIGH);
  }
}

void setupDisplay() {
  Serial.println("Initializing OLED display...");
  if (!oled.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    Serial.println("  ✗ SSD1306 allocation failed");
    while (true);
  }
  Serial.println("  ✓ OLED display initialized.");
}

void setupMPU() {
  Serial.println("Initializing MPU6050...");

  byte status = mpu.begin();
  Serial.printf("  MPU6050 status: %d\n", status);
  while (status != 0) {
  }  // stop everything if could not connect to MPU6050

  Serial.println("Calculating MPU6050 offsets, do not move MPU6050");
  mpu.calcOffsets(CALCULATE_OFFSET_GYRO, CALCULATE_OFFSET_ACCEL);
  Serial.println("  ✓ MPU6050 initialized.");
}

void renderCalibrationSetup() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 10);
  oled.println("Calibrating Offsets.");
  oled.setCursor(0, 25);
  oled.println("Do not move...");
  oled.display();
}

void renderOrientation() {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 10);
  oled.println("Roll: " + String((int)mpu.getAngleX() * -1));
  oled.setCursor(0, 25);
  oled.println("Pitch: " + String((int)mpu.getAngleY()));
  oled.setCursor(0, 40);
  oled.println("Yaw: " + String((int)mpu.getAngleZ() * -1));
  oled.display();
}