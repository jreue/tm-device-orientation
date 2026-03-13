#pragma once

#define NUM_PLAYERS 2  // (1=Master only, 2=Master+1, 3=Master+2)
#define NUM_PHASES 3   // Number of orientation phases players must complete before transmission
#define ORIENTATION_TOLERANCE 2  // degrees of tolerance for matching orientation targets

#define MASTER_DEVICE_ID 102
#define SLAVE_DEVICE_ID_1 112
#define SLAVE_DEVICE_ID_2 113

// ====================
// This Devices Configuration
// ====================
// #define DEVICE_ROLE_MASTER
#define DEVICE_ROLE_SLAVE

#ifdef DEVICE_ROLE_MASTER
#define DEVICE_ID MASTER_DEVICE_ID
#else
#define DEVICE_ID SLAVE_DEVICE_ID_1
#endif

// ====================
// Button Configuration
// ====================
#define SUBMIT_PHASE_BUTTON_PIN GPIO_NUM_5
#define RESET_OFFSETS_BUTTON_PIN GPIO_NUM_17  // TX2
#define TRANSMIT_BUTTON_PIN GPIO_NUM_16       // RX2
#define LOAD_PHASE_BUTTON_PIN GPIO_NUM_4

// ====================
// LED Configuration
// ====================
#define LED_PHASE_1_SUCCESS_PIN GPIO_NUM_27
#define LED_PHASE_2_SUCCESS_PIN GPIO_NUM_26
#define LED_PHASE_3_SUCCESS_PIN GPIO_NUM_25
#define LED_TRANSMITTED_PIN GPIO_NUM_32

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