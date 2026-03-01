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

Adafruit_SSD1306 oled(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET);
MPU6050 mpu(Wire);

uint8_t hubAddress[] = HUB_MAC_ADDRESS;
uint8_t orientiationMasterAddress[] = ORIENTATION_MASTER_MAC_ADDRESS;
uint8_t orientationSlave1Address[] = ORIENTATION_SLAVE_1_MAC_ADDRESS;

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
    {0, 0, 10},  // Round 1
    {0, 0, 20},  // Round 2
    {0, 0, 10}   // Round 3
};

Orientation currentOrientation = {0, 0, 0};
int currentRound = 0;
bool roundCompleted[TOTAL_ROUNDS] = {false, false, false};

bool isRoundStaged = false;
bool isRoundLoading = false;
bool isProcessing = false;
bool isCalibrationStaged = false;
bool isCalibrationComplete = false;
bool isMasterWaiting = false;
bool isSlaveWaiting = false;

const int NUM_PLAYERS = 2;  // Master + 1 Slave
struct PlayerSubmission {
    uint8_t deviceId;
    bool success;
};
// Track player submissions for the current round
PlayerSubmission playerSubmissions[NUM_PLAYERS] = {
    {102, false},  // Master player
    {112, false}   // Slave player
};

void setupDisplay();
void setupMPU();
void setCurrentOrientation();
bool orientationMatches(const Orientation& target, int x, int y, int z);

void handleMasterOrientationProgressMessage(const OrientationProgressMessage& message);
void handleSlaveOrientationMessage(const OrientationSubmissionMessage& message);

void handleMasterOrientationMatched();
void handleSlaveOrientationMatched(int deviceId);

void handleSubmitButtonPressed(void* button_handle, void* usr_data);
void handleMasterRoundProgressButtonPressed(void* button_handle, void* usr_data);
void handleMasterCalibrateButtonPressed(void* button_handle, void* usr_data);

void updateRoundLEDs();

void renderCalibrationSetup();
void renderOrientation();
void renderRoundStaged();
void renderRoundLoading();
void renderCalibrationStaged();
void renderCalibrationComplete();
void renderSlaveWaitScreen();
void renderMasterWaitScreen();

void stageRound();
void loadRound();
void completeRound();
void stageCalibration();
void completeCalibration();
void waitForMaster();
void waitForSlaves();

void submitAndPossiblyCompleteRound(uint8_t deviceId);
void setPlayerSubmission(uint8_t deviceId);
bool allPlayersSubmitted();
void resetPlayerSubmissions();
bool isCalibrated();

void playSuccessMelody();
void playTriumphMelody();

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

#ifdef DEVICE_ROLE_MASTER
  Serial.println("Device role: MASTER");
  espNowHelper.addPeer(hubAddress);
  espNowHelper.addPeer(orientationSlave1Address);
  espNowHelper.sendModuleConnected(hubAddress);

  espNowHelper.registerOrientationMessageHandler(&handleSlaveOrientationMessage);
#endif

#ifdef DEVICE_ROLE_SLAVE
  Serial.println("Device role: SLAVE");
  espNowHelper.addPeer(orientiationMasterAddress);

  espNowHelper.registerOrientationProgressMessageHandler(&handleMasterOrientationProgressMessage);
#endif

  Wire.begin();

  pinMode(LED_ROUND_1_SUCCESS_PIN, OUTPUT);
  pinMode(LED_ROUND_2_SUCCESS_PIN, OUTPUT);
  pinMode(LED_ROUND_3_SUCCESS_PIN, OUTPUT);
  pinMode(LED_CALIBRATED_PIN, OUTPUT);

  digitalWrite(LED_ROUND_1_SUCCESS_PIN, LOW);
  digitalWrite(LED_ROUND_2_SUCCESS_PIN, LOW);
  digitalWrite(LED_ROUND_3_SUCCESS_PIN, LOW);
  digitalWrite(LED_CALIBRATED_PIN, LOW);

  digitalWrite(BUZZER_PIN, LOW);

  delay(2000);

  setupDisplay();

  renderCalibrationSetup();

  delay(1000);

  setupMPU();

  delay(1000);

  Button* btn = new Button(SUBMIT_BUTTON_PIN, false);
  btn->attachPressDownEventCb(&handleSubmitButtonPressed, NULL);

#ifdef DEVICE_ROLE_MASTER
  Serial.println("Setting up master round progress button...");
  Button* masterRoundProgressButton = new Button(ROUND_BUTTON_PIN, false);
  masterRoundProgressButton->attachPressDownEventCb(&handleMasterRoundProgressButtonPressed, NULL);

  Serial.println("Setting up master calibrate button...");
  Button* masterCalibrateButton = new Button(CALIBRATE_BUTTON_PIN, false);
  masterCalibrateButton->attachPressDownEventCb(&handleMasterCalibrateButtonPressed, NULL);
#endif

#ifdef DEVICE_ROLE_MASTER
  stageRound();
#endif
#ifdef DEVICE_ROLE_SLAVE
  isSlaveWaiting = true;
  renderSlaveWaitScreen();
#endif
}

void loop() {
  mpu.update();

  if (isRoundStaged) {
    return;  // Skip rendering if we are waiting for the next round to start
  }

  if (isRoundLoading) {
    return;  // Skip rendering if a round is loading
  }

  if (isCalibrationStaged) {
    return;  // Skip rendering if calibration is staged but not completed
  }

  if (isCalibrationComplete) {
    return;  // Skip rendering if calibration is completed
  }

  if (isMasterWaiting) {
    return;  // Skip rendering if master is waiting for slaves to submit
  }

  if (isSlaveWaiting) {
    return;  // Skip rendering if slave is waiting for master
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
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
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

void handleSlaveOrientationMessage(const OrientationSubmissionMessage& message) {
  Serial.printf("Received orientation message from slave module: %d\n", message.deviceId);
  Serial.printf("  Roll: %d, Pitch: %d, Yaw: %d\n", message.roll, message.pitch, message.yaw);

  // We are assuming any message from slave is a successful orientation match for the current round
  // since slave only sends when it matches
  handleSlaveOrientationMatched(message.deviceId);
}

// Master -> Slave: Master reports round progress and clears isSlaveWaiting to allow slave to start
// next round
void handleMasterOrientationProgressMessage(const OrientationProgressMessage& message) {
  Serial.printf("Received orientation progress message from master: %d%%\n", message.round);

  currentRound = message.round;
  isSlaveWaiting = false;

  loadRound();
}

void handleMasterOrientationMatched() {
  Serial.printf("Master reported orientation match for device %d!\n", DEVICE_ID);
  // Additional logic for when orientation matches can be added here
  submitAndPossiblyCompleteRound(DEVICE_ID);
}

void handleSlaveOrientationMatched(int deviceId) {
  Serial.printf("Slave reported orientation match for device %d!\n", deviceId);
  // Additional logic for when slave reports orientation match can be added here
  submitAndPossiblyCompleteRound(deviceId);
}

void submitAndPossiblyCompleteRound(uint8_t deviceId) {
  setPlayerSubmission(deviceId);

  if (allPlayersSubmitted()) {
    Serial.println("All players submitted successfully for this round!");
    completeRound();

    if (currentRound >= TOTAL_ROUNDS) {
      stageCalibration();
      return;
    }

    stageRound();
  } else {
    Serial.println("Waiting for all players to submit...");
  }
}

void setPlayerSubmission(uint8_t deviceId) {
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (playerSubmissions[i].deviceId == deviceId) {
      playerSubmissions[i].success = true;
      break;
    }
  }
}
bool allPlayersSubmitted() {
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (!playerSubmissions[i].success) {
      return false;
    }
  }
  return true;
}

void resetPlayerSubmissions() {
  for (int i = 0; i < NUM_PLAYERS; i++) {
    playerSubmissions[i].success = false;
  }
}

void stageRound() {
  isRoundStaged = true;
  renderRoundStaged();
}

void loadRound() {
  isRoundStaged = false;
  isRoundLoading = true;
  isSlaveWaiting = false;
  isMasterWaiting = false;

#ifdef DEVICE_ROLE_MASTER
  espNowHelper.sendOrientationProgressUpdated(orientationSlave1Address, currentRound);
#endif

  renderRoundLoading();
  isRoundLoading = false;
}

void completeRound() {
  resetPlayerSubmissions();

  roundCompleted[currentRound] = true;
  currentRound++;
  updateRoundLEDs();

  playSuccessMelody();
}

void stageCalibration() {
  isCalibrationStaged = true;
  renderCalibrationStaged();
}

void completeCalibration() {
  isCalibrationStaged = false;
  isCalibrationComplete = true;

  renderCalibrationComplete();
  playTriumphMelody();
  digitalWrite(LED_CALIBRATED_PIN, HIGH);
  espNowHelper.sendModuleUpdated(hubAddress, true);
}

void waitForSlaves() {
  isMasterWaiting = true;
  renderMasterWaitScreen();
}

void waitForMaster() {
  isSlaveWaiting = true;
  renderSlaveWaitScreen();
}

void handleSubmitButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Submit button pressed");
  if (currentRound >= TOTAL_ROUNDS) {
    return;
  }

  int x = currentOrientation.x;
  int y = currentOrientation.y;
  int z = currentOrientation.z;
  Serial.printf("Current orientation: x=%d, y=%d, z=%d\n", x, y, z);

  const Orientation& target = roundTargets[currentRound];
  if (orientationMatches(target, x, y, z)) {
#ifdef DEVICE_ROLE_MASTER
    waitForSlaves();
    handleMasterOrientationMatched();
#endif
#ifdef DEVICE_ROLE_SLAVE
    espNowHelper.sendOrientationUpdated(orientiationMasterAddress, x, y, z, currentRound, true);
    waitForMaster();
#endif
  } else {
    Serial.printf("Round %d not matched. Try again.\n", currentRound + 1);
  }
}

void handleMasterRoundProgressButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master round progress button pressed");
  loadRound();
}

void handleMasterCalibrateButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master calibrate button pressed");
  if (isCalibrated()) {
    completeCalibration();
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
  oled.setCursor(10, 24);
  oled.print("< Tuning Offsets >");
  oled.setCursor(42, 40);
  oled.print("WAIT...");
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

void renderRoundStaged() {
  oled.clearDisplay();

  // Render "Start Round" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      11 * 6;  // Approximate width: 6 pixels per character, "Start Round" is 11 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Start Round");

  oled.display();
}

void renderRoundLoading() {
  int roundNumber = currentRound + 1;  // Display rounds starting from 1 instead of 0

  oled.clearDisplay();

  // Center "Round X" text on the horizontal axis
  oled.setTextSize(2);  // Make the "Round X" text bigger
  int roundTextWidth =
      12 * 6;  // Approximate width: 6 pixels per character, "Round X" is 12 characters max
  oled.setCursor((OLED_SCREEN_WIDTH - roundTextWidth) / 2, 10);
  oled.printf("Round %d", roundNumber);

  for (int countdown = ROUND_START_COUNTDOWN; countdown > 0; --countdown) {
    oled.setTextSize(4);
    oled.setCursor((OLED_SCREEN_WIDTH - 24) / 2,
                   (OLED_SCREEN_HEIGHT - 32) / 2 + 10);  // Move the number down slightly
    oled.printf("%d", countdown);
    oled.display();
    delay(1000);

    oled.fillRect((OLED_SCREEN_WIDTH - 24) / 2, (OLED_SCREEN_HEIGHT - 32) / 2 + 10, 24, 32,
                  BLACK);  // Clear the number
  }

  oled.clearDisplay();
  oled.display();
}

void renderCalibrationStaged() {
  oled.clearDisplay();

  // Render "Completed" centered
  oled.setTextSize(2);
  int completedTextWidth =
      9 * 12;  // Approximate width: 12 pixels per character, "Completed" is 9 characters
  oled.setCursor((OLED_SCREEN_WIDTH - completedTextWidth) / 2, 10);
  oled.println("Completed");

  // Render "Submit Calibration" centered below
  oled.setTextSize(1);
  int submitTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Submit Calibration" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - submitTextWidth) / 2, 40);
  oled.println("Submit Calibration");

  oled.display();
}

void renderCalibrationComplete() {
  oled.clearDisplay();

  // Render "Calibration Complete" centered
  oled.setTextSize(1);
  int completeTextWidth =
      20 * 6;  // Approximate width: 6 pixels per character, "Calibration Complete" is 20 characters
  oled.setCursor((OLED_SCREEN_WIDTH - completeTextWidth) / 2, 24);
  oled.println("Calibration Complete");

  oled.display();
}

void renderSlaveWaitScreen() {
  oled.clearDisplay();

  // Render "Waiting for Master" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Waiting for Master" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Waiting for Master");

  oled.display();
}

void renderMasterWaitScreen() {
  oled.clearDisplay();

  // Render "Waiting for Slaves" centered
  oled.setTextSize(1);
  int waitingTextWidth =
      18 * 6;  // Approximate width: 6 pixels per character, "Waiting for Slaves" is 18 characters
  oled.setCursor((OLED_SCREEN_WIDTH - waitingTextWidth) / 2, 40);
  oled.println("Waiting for Slaves");

  oled.display();
}

void playSuccessMelody() {
  // Play a simple success melody using the buzzer
  tone(BUZZER_PIN, 1000, 200);  // Play 1000 Hz for 200 ms
  delay(250);
  tone(BUZZER_PIN, 1500, 200);  // Play 1500 Hz for 200 ms
  delay(250);
  tone(BUZZER_PIN, 2000, 300);  // Play 2000 Hz for 300 ms
}

void playTriumphMelody() {
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