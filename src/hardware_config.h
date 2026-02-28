#pragma once

// ====================
// This Devices Configuration
// ====================
#define DEVICE_ID 102

// Master 102
// Slave 112
// Slave 113

#define DEVICE_ROLE_MASTER
// #define DEVICE_ROLE_SLAVE

// ====================
// Button Configuration
// ====================
#define SUBMIT_BUTTON_PIN GPIO_NUM_5
#define ROUND_BUTTON_PIN GPIO_NUM_16
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
// Buzzer Configuration
#define BUZZER_PIN GPIO_NUM_14

// ====================
// MPU6050 Configuration
// ====================
#define CALCULATE_OFFSET_GYRO true
#define CALCULATE_OFFSET_ACCEL true