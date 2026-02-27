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

bool isCalibrated() {
  return false;
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
  Button* masterCalibrateButton = new Button(CALIBRATE_BUTTON_PIN, false);

  btn->attachPressDownEventCb(&onSubmitButtonPressDownCb, NULL);
  masterCalibrateButton->attachPressDownEventCb(&onMasterCalibrateButtonPressDownCb, NULL);
}

void loop() {
  mpu.update();

  renderOrientation();
}

void onSubmitButtonPressDownCb(void* button_handle, void* usr_data) {
  Serial.println("Button pressed down");
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