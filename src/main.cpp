#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_light.h>
#include <shared_hardware_config.h>

#include "EspNowHelper.h"
#include "Wire.h"
#include "hardware_config.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MPU6050 mpu(Wire);

uint8_t hubAddress[] = HUB_MAC_ADDRESS;
EspNowHelper espNowHelper;
unsigned long timer = 0;
const unsigned long ORIENTATION_DISPLAY_INTERVAL_MS = 100;

void setupDisplay();
void renderCalibrationSetup();
void renderOrientation();
void setupMPU();

bool isCalibrated() {
  return false;
}

void setup() {
  Serial.begin(115200);

  espNowHelper.begin(DEVICE_ID);
  espNowHelper.addPeer(hubAddress);
  espNowHelper.sendModuleConnected(hubAddress);

  Wire.begin();

  delay(2000);

  setupDisplay();

  renderCalibrationSetup();

  delay(1000);

  setupMPU();

  delay(1000);
}

void loop() {
  mpu.update();

  if ((millis() - timer) > ORIENTATION_DISPLAY_INTERVAL_MS) {
    renderOrientation();

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