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

// Phase target definitions
struct Orientation {
    int x;
    int y;
    int z;
};

const int TOTAL_PHASES = 3;
const int PHASE_START_COUNTDOWN = 5;

const Orientation phaseTargets[TOTAL_PHASES] = {
    {0, 0, 10},  // Phase 1
    {0, 0, 20},  // Phase 2
    {0, 0, 10},  // Phase 3
};

Orientation currentOrientation = {0, 0, 0};
int currentPhase = 0;
bool phaseCompleted[TOTAL_PHASES] = {false, false, false};

const int NUM_PLAYERS = 2;  // Master + 1 Slave
struct PlayerSubmission {
    uint8_t deviceId;
    bool success;
};
// Track player submissions for the current phase
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

void handleSubmitPhaseButtonPressed(void* button_handle, void* usr_data);
void handleLoadPhaseButtonPressed(void* button_handle, void* usr_data);
void handleTransmitButtonPressed(void* button_handle, void* usr_data);

void updatePhaseLEDs();

void initialize();
void stagePhase();
void loadPhase();
void completePhase();
void stageTransmit();
void completeTransmit();
void waitForMaster();
void waitForSlaves();
void stageInvalidSubmission();

void transitionToState(const int state);

void submitAndPossiblyCompletePhase(uint8_t deviceId);
void setPlayerSubmission(uint8_t deviceId);
bool allPlayersSubmitted();
void resetPlayerSubmissions();
bool isCalibrated();

// Returns true if all phases are completed
bool isCalibrated() {
  for (int i = 0; i < TOTAL_PHASES; i++) {
    if (!phaseCompleted[i]) {
      return false;
    }
  }
  return true;
}

const int STATE_INITIALIZING = 0;
const int STATE_PHASE_STAGED = 1;
const int STATE_PHASE_LOADING = 2;
const int STATE_PROCESSING = 3;
const int STATE_MASTER_WAITING = 4;
const int STATE_SLAVE_WAITING = 5;
const int STATE_TRANSMIT_STAGED = 6;
const int STATE_TRANSMIT_COMPLETE = 7;
const int STATE_INVALID_SUBMISSION = 8;

int currentState = STATE_INITIALIZING;

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

  pinMode(LED_PHASE_1_SUCCESS_PIN, OUTPUT);
  pinMode(LED_PHASE_2_SUCCESS_PIN, OUTPUT);
  pinMode(LED_PHASE_3_SUCCESS_PIN, OUTPUT);
  pinMode(LED_TRANSMITTED_PIN, OUTPUT);

  digitalWrite(LED_PHASE_1_SUCCESS_PIN, LOW);
  digitalWrite(LED_PHASE_2_SUCCESS_PIN, LOW);
  digitalWrite(LED_PHASE_3_SUCCESS_PIN, LOW);
  digitalWrite(LED_TRANSMITTED_PIN, LOW);

  digitalWrite(BUZZER_PIN, LOW);

  delay(2000);

  setupDisplay();

  initialize();

  delay(1000);

  setupMPU();

  delay(1000);

  Button* btn = new Button(SUBMIT_PHASE_BUTTON_PIN, false);
  btn->attachPressDownEventCb(&handleSubmitPhaseButtonPressed, NULL);

#ifdef DEVICE_ROLE_MASTER
  Serial.println("Setting up master load phase button...");
  Button* masterLoadPhaseButton = new Button(LOAD_PHASE_BUTTON_PIN, false);
  masterLoadPhaseButton->attachPressDownEventCb(&handleLoadPhaseButtonPressed, NULL);

  Serial.println("Setting up master transmit button...");
  Button* masterTransmitButton = new Button(TRANSMIT_BUTTON_PIN, false);
  masterTransmitButton->attachPressDownEventCb(&handleTransmitButtonPressed, NULL);
#endif

#ifdef DEVICE_ROLE_MASTER
  stagePhase();
#endif
#ifdef DEVICE_ROLE_SLAVE
  waitForMaster();
#endif
}

void loop() {
  mpu.update();

  if (currentState != STATE_PROCESSING) {
    return;  // Skip processing if we are in a non-processing state
  }

  if ((millis() - timer) > ORIENTATION_DISPLAY_INTERVAL_MS) {
    setCurrentOrientation();
    OLEDController::renderOrientation(oled, currentOrientation.x, currentOrientation.y,
                                      currentOrientation.z);

    updatePhaseLEDs();
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
  currentState = state;
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

  // We are assuming any message from slave is a successful orientation match for the current phase
  // since slave only sends when it matches
  handleSlaveOrientationMatched(message.deviceId);
}

// Master -> Slave: Master reports phase progress
void handleMasterOrientationProgressMessage(const OrientationProgressMessage& message) {
  Serial.printf("Received orientation progress message from master: %d%%\n", message.round);

  currentPhase = message.round;

  if (message.isFinalized) {
    Serial.println("All Phases are done. Master transmitted final calibration to HUB.");
    transitionToState(STATE_TRANSMIT_COMPLETE);
    OLEDController::renderTransmitComplete(oled);
    return;
  } else {
    transitionToState(STATE_SLAVE_WAITING);

    loadPhase();
  }
}

void handleMasterOrientationMatched() {
  Serial.printf("Master reported orientation match for device %d!\n", DEVICE_ID);
  submitAndPossiblyCompletePhase(DEVICE_ID);
}

void handleSlaveOrientationMatched(int deviceId) {
  Serial.printf("Slave reported orientation match for device %d!\n", deviceId);
  submitAndPossiblyCompletePhase(deviceId);
}

void submitAndPossiblyCompletePhase(uint8_t deviceId) {
  setPlayerSubmission(deviceId);

  if (allPlayersSubmitted()) {
    Serial.println("All players submitted successfully for this phase!");
    completePhase();

    if (currentPhase >= TOTAL_PHASES) {
      stageTransmit();
      return;
    }

    stagePhase();
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

void stagePhase() {
  transitionToState(STATE_PHASE_STAGED);
  OLEDController::renderPhaseStaged(oled, currentPhase, TOTAL_PHASES);
}

void loadPhase() {
  transitionToState(STATE_PHASE_LOADING);
#ifdef DEVICE_ROLE_MASTER
  espNowHelper.sendOrientationProgressUpdated(orientationSlave1Address, currentPhase, false);
#endif

  OLEDController::renderPhaseLoading(oled, currentPhase, PHASE_START_COUNTDOWN);
  transitionToState(STATE_PROCESSING);
}

void completePhase() {
  resetPlayerSubmissions();

  phaseCompleted[currentPhase] = true;
  currentPhase++;
  updatePhaseLEDs();

  BuzzerController::playSuccessMelody();
}

void stageTransmit() {
  transitionToState(STATE_TRANSMIT_STAGED);
  OLEDController::renderTransmitStaged(oled);
}

void completeTransmit() {
  transitionToState(STATE_TRANSMIT_COMPLETE);
  OLEDController::renderTransmitComplete(oled);

  BuzzerController::playTriumphMelody();
  digitalWrite(LED_TRANSMITTED_PIN, HIGH);

  espNowHelper.sendModuleUpdated(hubAddress, true);
  espNowHelper.sendOrientationProgressUpdated(orientationSlave1Address, currentPhase, true);
}

void waitForSlaves() {
  transitionToState(STATE_MASTER_WAITING);
  OLEDController::renderMasterWaitScreen(oled);
}

void waitForMaster() {
  transitionToState(STATE_SLAVE_WAITING);
  OLEDController::renderSlaveWaitScreen(oled);
}

void stageInvalidSubmission() {
  transitionToState(STATE_INVALID_SUBMISSION);
  OLEDController::renderInvalidSubmissionScreen(oled);
  transitionToState(STATE_PROCESSING);
}

void handleSubmitPhaseButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Submit phase button pressed");

  if (currentState != STATE_PROCESSING) {
    return;
  }

  if (currentPhase >= TOTAL_PHASES) {
    return;
  }

  int x = currentOrientation.x;
  int y = currentOrientation.y;
  int z = currentOrientation.z;
  Serial.printf("Current orientation: x=%d, y=%d, z=%d\n", x, y, z);

  const Orientation& target = phaseTargets[currentPhase];
  if (orientationMatches(target, x, y, z)) {
#ifdef DEVICE_ROLE_MASTER
    waitForSlaves();
    handleMasterOrientationMatched();
#endif
#ifdef DEVICE_ROLE_SLAVE
    espNowHelper.sendOrientationUpdated(orientiationMasterAddress, x, y, z, currentPhase, true);
    waitForMaster();
#endif
  } else {
    Serial.printf("Phase %d not matched. Try again.\n", currentPhase + 1);
    stageInvalidSubmission();
  }
}

void handleLoadPhaseButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master load phase button pressed");

  if (currentState != STATE_PHASE_STAGED) {
    return;
  }

  loadPhase();
}

void handleTransmitButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master transmit button pressed");

  if (currentState != STATE_TRANSMIT_STAGED) {
    return;
  }

  if (isCalibrated()) {
    completeTransmit();
  }
}

void updatePhaseLEDs() {
  digitalWrite(LED_PHASE_1_SUCCESS_PIN, phaseCompleted[0] ? HIGH : LOW);
  digitalWrite(LED_PHASE_2_SUCCESS_PIN, phaseCompleted[1] ? HIGH : LOW);
  digitalWrite(LED_PHASE_3_SUCCESS_PIN, phaseCompleted[2] ? HIGH : LOW);
}