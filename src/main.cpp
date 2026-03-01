#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <MPU6050_light.h>
#include <shared_hardware_config.h>
#include <stdint.h>

#include "Button.h"
#include "BuzzerController.h"
#include "EspNowHelper.h"
#include "OLEDController.h"
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

bool isInitializing = false;
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

void initialize();
void stageRound();
void loadRound();
void completeRound();
void stageCalibration();
void completeCalibration();
void waitForMaster();
void waitForSlaves();

void transitionToState(const int state);

void submitAndPossiblyCompleteRound(uint8_t deviceId);
void setPlayerSubmission(uint8_t deviceId);
bool allPlayersSubmitted();
void resetPlayerSubmissions();
bool isCalibrated();

// Returns true if all rounds are completed
bool isCalibrated() {
  for (int i = 0; i < TOTAL_ROUNDS; i++) {
    if (!roundCompleted[i]) {
      return false;
    }
  }
  return true;
}

const int STATE_INITIALIZING = 0;
const int STATE_ROUND_STAGED = 1;
const int STATE_ROUND_LOADING = 2;
const int STATE_PROCESSING = 3;
const int STATE_MASTER_WAITING = 4;
const int STATE_SLAVE_WAITING = 5;
const int STATE_CALIBRATION_STAGED = 6;
const int STATE_CALIBRATION_COMPLETE = 7;

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

  initialize();

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
  transitionToState(STATE_SLAVE_WAITING);
  OLEDController::renderSlaveWaitScreen(oled);
#endif
}

void loop() {
  mpu.update();

  if (!isProcessing) {
    return;  // Skip processing if we are in a non-processing state
  }

  if ((millis() - timer) > ORIENTATION_DISPLAY_INTERVAL_MS) {
    setCurrentOrientation();
    OLEDController::renderOrientation(oled, currentOrientation.x, currentOrientation.y,
                                      currentOrientation.z);

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

void transitionToState(const int state) {
  Serial.println("-----------------------------------");
  Serial.printf("➤ ➤ Transitioning to state: %d\n", state);
  Serial.println("-----------------------------------");
  isInitializing = state == STATE_INITIALIZING;
  isProcessing = state == STATE_PROCESSING;
  isRoundStaged = state == STATE_ROUND_STAGED;
  isRoundLoading = state == STATE_ROUND_LOADING;
  isCalibrationStaged = state == STATE_CALIBRATION_STAGED;
  isCalibrationComplete = state == STATE_CALIBRATION_COMPLETE;
  isMasterWaiting = state == STATE_MASTER_WAITING;
  isSlaveWaiting = state == STATE_SLAVE_WAITING;
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
  transitionToState(STATE_SLAVE_WAITING);

  loadRound();
}

void handleMasterOrientationMatched() {
  Serial.printf("Master reported orientation match for device %d!\n", DEVICE_ID);
  submitAndPossiblyCompleteRound(DEVICE_ID);
}

void handleSlaveOrientationMatched(int deviceId) {
  Serial.printf("Slave reported orientation match for device %d!\n", deviceId);
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

void initialize() {
  transitionToState(STATE_INITIALIZING);
  OLEDController::renderCalibrationSetup(oled);
}

void stageRound() {
  transitionToState(STATE_ROUND_STAGED);
  OLEDController::renderRoundStaged(oled);
}

void loadRound() {
  transitionToState(STATE_ROUND_LOADING);
#ifdef DEVICE_ROLE_MASTER
  espNowHelper.sendOrientationProgressUpdated(orientationSlave1Address, currentRound);
#endif

  OLEDController::renderRoundLoading(oled, currentRound, ROUND_START_COUNTDOWN);
  transitionToState(STATE_PROCESSING);
}

void completeRound() {
  resetPlayerSubmissions();

  roundCompleted[currentRound] = true;
  currentRound++;
  updateRoundLEDs();

  BuzzerController::playSuccessMelody();
}

void stageCalibration() {
  transitionToState(STATE_CALIBRATION_STAGED);
  OLEDController::renderCalibrationStaged(oled);
}

void completeCalibration() {
  transitionToState(STATE_CALIBRATION_COMPLETE);

  OLEDController::renderCalibrationComplete(oled);
  BuzzerController::playTriumphMelody();
  digitalWrite(LED_CALIBRATED_PIN, HIGH);
  espNowHelper.sendModuleUpdated(hubAddress, true);
}

void waitForSlaves() {
  transitionToState(STATE_MASTER_WAITING);
  OLEDController::renderMasterWaitScreen(oled);
}

void waitForMaster() {
  transitionToState(STATE_SLAVE_WAITING);
  OLEDController::renderSlaveWaitScreen(oled);
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