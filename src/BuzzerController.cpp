#include "BuzzerController.h"

#include <Arduino.h>
#include <esp32-hal-ledc.h>

#include "hardware_config.h"

// Use a fixed LEDC channel to avoid conflicts with FastLED (which uses RMT)
// and the auto-allocation in tone() which breaks on ESP32 Arduino core v3.x after RMT init.
#define BUZZER_LEDC_CHANNEL 4
#define BUZZER_LEDC_RESOLUTION 8

static void playTone(int freq, int duration_ms) {
  ledcSetup(BUZZER_LEDC_CHANNEL, freq, BUZZER_LEDC_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
  ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
  delay(duration_ms);
  ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
  ledcDetachPin(BUZZER_PIN);
}

void BuzzerController::playSuccessMelody() {
  playTone(1000, 200);
  delay(250);
  playTone(1500, 200);
  delay(250);
  playTone(2000, 300);
}

void BuzzerController::playTriumphMelody() {
  playTone(1000, 200);
  delay(250);
  playTone(1200, 200);
  delay(250);
  playTone(1500, 300);
  delay(350);
  playTone(2000, 400);
  delay(450);
  playTone(1000, 200);
  delay(250);
  playTone(1200, 200);
  delay(250);
  playTone(1500, 300);
  delay(350);
  playTone(2000, 400);
  delay(450);
}