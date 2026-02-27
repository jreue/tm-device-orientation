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

unsigned long timer = 0;
const unsigned long ORIENTATION_DISPLAY_INTERVAL_MS = 100;
const int ORIENTATION_TOLERANCE = 2;  // degrees of tolerance for matching orientation targets

// Round target definitions
struct Orientation {
    int x;
    int y;
    int z;
};

const int TOTAL_ROUNDS = 3;
const int ROUND_START_COUNTDOWN = 5;

const Orientation roundTargets[TOTAL_ROUNDS] = {
    {0, 0, 10},    // Round 1
    {10, 10, 20},  // Round 2
    {20, 20, 0}    // Round 3
};

Orientation currentOrientation = {0, 0, 0};
int currentRound = 0;
bool roundCompleted[TOTAL_ROUNDS] = {false, false, false};
bool isStartingRound = false;

void setupDisplay();
void setupMPU();
void setCurrentOrientation();
bool orientationMatches(const Orientation& target, int x, int y, int z);
void onSubmitButtonPressed(void* button_handle, void* usr_data);
void onMasterCalibrateButtonPressed(void* button_handle, void* usr_data);
void updateRoundLEDs();
void renderCalibrationSetup();
void renderOrientation();
void renderRoundStart(int roundNumber);
void startRound(int roundNumber);
void completeRound();

// Returns true if all rounds are completed
bool isCalibrated() {
  for (int i = 0; i < TOTAL_ROUNDS; i++) {
    if (!roundCompleted[i]) {
      return false;
    }
  }
  return true;
}

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
  btn->attachPressDownEventCb(&onSubmitButtonPressed, NULL);

#if DEVICE_ROLE == DEVICE_ROLE_MASTER
  Button* masterCalibrateButton = new Button(CALIBRATE_BUTTON_PIN, false);
  masterCalibrateButton->attachPressDownEventCb(&onMasterCalibrateButtonPressed, NULL);
#endif

  // Call startRound to display the round loading screen
  startRound(currentRound + 1);
}

void loop() {
  mpu.update();

  if (isStartingRound) {
    return;  // Skip rendering if a round is starting
  }

  if ((millis() - timer) > ORIENTATION_DISPLAY_INTERVAL_MS) {
    setCurrentOrientation();
    renderOrientation();

    updateRoundLEDs();
    timer = millis();
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

void setCurrentOrientation() {
  currentOrientation.x = (int)mpu.getAngleX() * -1;
  currentOrientation.y = (int)mpu.getAngleY();
  currentOrientation.z = (int)mpu.getAngleZ() * -1;
}

bool orientationMatches(const Orientation& target, int x, int y, int z) {
  return abs(target.x - x) <= ORIENTATION_TOLERANCE && abs(target.y - y) <= ORIENTATION_TOLERANCE &&
         abs(target.z - z) <= ORIENTATION_TOLERANCE;
}

void onSubmitButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Submit button pressed down");
  if (currentRound >= TOTAL_ROUNDS) {
    return;
  }

  int x = currentOrientation.x;
  int y = currentOrientation.y;
  int z = currentOrientation.z;
  Serial.printf("Current orientation: x=%d, y=%d, z=%d\n", x, y, z);

  const Orientation& target = roundTargets[currentRound];
  if (orientationMatches(target, x, y, z)) {
    completeRound();
    startRound(currentRound);
  } else {
    Serial.printf("Round %d not matched. Try again.\n", currentRound + 1);
  }
}

void onMasterCalibrateButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master calibrate button pressed down");
  if (isCalibrated()) {
    digitalWrite(LED_CALIBRATED_PIN, HIGH);
  }
}

void updateRoundLEDs() {
  digitalWrite(LED_ROUND_1_SUCCESS_PIN, roundCompleted[0] ? HIGH : LOW);
  digitalWrite(LED_ROUND_2_SUCCESS_PIN, roundCompleted[1] ? HIGH : LOW);
  digitalWrite(LED_ROUND_3_SUCCESS_PIN, roundCompleted[2] ? HIGH : LOW);
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
  oled.println("Roll: " + String(currentOrientation.x));
  oled.setCursor(0, 25);
  oled.println("Pitch: " + String(currentOrientation.y));
  oled.setCursor(0, 40);
  oled.println("Yaw: " + String(currentOrientation.z));
  oled.display();
}

void renderRoundStart(int roundNumber) {
  oled.clearDisplay();

  // Center "Round X" text on the horizontal axis
  oled.setTextSize(2);  // Make the "Round X" text bigger
  int roundTextWidth =
      12 * 6;  // Approximate width: 6 pixels per character, "Round X" is 12 characters max
  oled.setCursor((SCREEN_WIDTH - roundTextWidth) / 2, 10);
  oled.printf("Round %d", roundNumber);

  for (int countdown = ROUND_START_COUNTDOWN; countdown > 0; --countdown) {
    oled.setTextSize(4);
    oled.setCursor((SCREEN_WIDTH - 24) / 2,
                   (SCREEN_HEIGHT - 32) / 2 + 10);  // Move the number down slightly
    oled.printf("%d", countdown);
    oled.display();
    delay(1000);

    oled.fillRect((SCREEN_WIDTH - 24) / 2, (SCREEN_HEIGHT - 32) / 2 + 10, 24, 32,
                  BLACK);  // Clear the number
  }

  oled.clearDisplay();
  oled.display();
}

void startRound(int roundNumber) {
  isStartingRound = true;
  renderRoundStart(roundNumber);
  isStartingRound = false;
}

void completeRound() {
  roundCompleted[currentRound] = true;
  currentRound++;
  updateRoundLEDs();
}