#pragma once

// ====================
// This Devices Configuration
// ====================
#define DEVICE_ID 104

#define SUBMIT_BUTTON_PIN GPIO_NUM_5
#define CALIBRATE_BUTTON_PIN GPIO_NUM_17

#define LED_ROUND_1_SUCCESS_PIN GPIO_NUM_18
#define LED_ROUND_2_SUCCESS_PIN GPIO_NUM_16
#define LED_ROUND_3_SUCCESS_PIN GPIO_NUM_23
#define LED_CALIBRATED_PIN GPIO_NUM_19

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define I2C_ADDRESS 0x3C
#define CALCULATE_OFFSET_GYRO true
#define CALCULATE_OFFSET_ACCEL true