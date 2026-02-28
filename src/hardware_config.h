#pragma once

// ====================
// This Devices Configuration
// ====================
#define DEVICE_ID 102

#define DEVICE_ROLE_MASTER 1
#define DEVICE_ROLE_SLAVE 2

#define DEVICE_ROLE DEVICE_ROLE_MASTER

// ====================
// Button Configuration
// ====================
#define SUBMIT_BUTTON_PIN GPIO_NUM_5
#define CALIBRATE_BUTTON_PIN GPIO_NUM_17

// ====================
// LED Configuration
// ====================
#define LED_ROUND_1_SUCCESS_PIN GPIO_NUM_27
#define LED_ROUND_2_SUCCESS_PIN GPIO_NUM_26
#define LED_ROUND_3_SUCCESS_PIN GPIO_NUM_25
#define LED_CALIBRATED_PIN GPIO_NUM_32

// ====================
// OLED Display Configuration
// ====================
#define OLED_SCREEN_WIDTH 128  // OLED display width, in pixels
#define OLED_SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1          // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_I2C_ADDRESS 0x3C

// ====================
// MPU6050 Configuration
// ====================
#define CALCULATE_OFFSET_GYRO true
#define CALCULATE_OFFSET_ACCEL true