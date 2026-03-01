#include "BuzzerController.h"

#include <Arduino.h>

#include "hardware_config.h"

void BuzzerController::playSuccessMelody() {
  // Play a simple success melody using the buzzer
  tone(BUZZER_PIN, 1000, 200);  // Play 1000 Hz for 200 ms
  delay(250);
  tone(BUZZER_PIN, 1500, 200);  // Play 1500 Hz for 200 ms
  delay(250);
  tone(BUZZER_PIN, 2000, 300);  // Play 2000 Hz for 300 ms
}

void BuzzerController::playTriumphMelody() {
  // Play a more elaborate triumph melody using the buzzer
  tone(BUZZER_PIN, 1000, 200);
  delay(250);
  tone(BUZZER_PIN, 1200, 200);
  delay(250);
  tone(BUZZER_PIN, 1500, 300);
  delay(350);
  tone(BUZZER_PIN, 2000, 400);
  delay(450);
  tone(BUZZER_PIN, 1000, 200);
  delay(250);
  tone(BUZZER_PIN, 1200, 200);
  delay(250);
  tone(BUZZER_PIN, 1500, 300);
  delay(350);
  tone(BUZZER_PIN, 2000, 400);
  delay(450);
}