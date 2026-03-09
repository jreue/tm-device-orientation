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

unsigned long orientationRefreshTimer = 0;
const unsigned long ORIENTATION_REFRESH_INTERVAL_MS = 100;

const int ORIENTATION_TOLERANCE = 2;  // degrees of tolerance for matching orientation targets

const int NUM_PHASES = 3;
const int NUM_PLAYERS = 2;  // Master + 1 Slave

const int COUNTDOWN_SECONDS_BOOT = 5;
const int COUNTDOWN_SECONDS_PHASE_START = 5;
const int COUNTDOWN_SECONDS_INVALID_SUBMISSION = 4;

const int STATE_BOOTING = -1;
const int STATE_OFFSETS_SETUP = 0;
const int STATE_PHASE_STAGED = 1;
const int STATE_PHASE_LOADING = 2;
const int STATE_PROCESSING = 3;
const int STATE_MASTER_WAITING = 4;
const int STATE_SLAVE_WAITING = 5;
const int STATE_TRANSMIT_STAGED = 6;
const int STATE_TRANSMIT_COMPLETE = 7;
const int STATE_INVALID_SUBMISSION = 8;

int currentState = STATE_BOOTING;
int currentPhase = 0;
bool phaseCompleted[NUM_PHASES] = {false, false, false};

struct Orientation {
    int x;
    int y;
    int z;
};

const Orientation phaseTargets[NUM_PHASES] = {
    {0, 0, 10},  // Phase 1
    {0, 0, 15},  // Phase 2
    {0, 0, 10}   // Phase 3
};

Orientation currentOrientation = {0, 0, 0};
float angleZOffset = 0.0f;

struct PlayerSubmission {
    uint8_t deviceId;
    bool success;
};

PlayerSubmission playerSubmissions[NUM_PLAYERS] = {
    {102, false},  // Master player
    {112, false}   // Slave player
};

void setupESPNow();
void setupDisplay();
void setupMPU();
void setupButtons();
void setupEffects();

void calculateOffsets();

void handleOffsetsButtonPressed(void* button_handle, void* usr_data);
void handleSubmitPhaseButtonPressed(void* button_handle, void* usr_data);
void handleLoadPhaseButtonPressed(void* button_handle, void* usr_data);
void handleTransmitButtonPressed(void* button_handle, void* usr_data);

void handleSubmissionMessageFromSlave(const OrientationSubmissionMessage& message);
void handlePhaseMessageFromMaster(const OrientationPhaseMessage& message);
void handleTransmissionMessageFromMaster(const OrientationTransmissionMessage& message);

const char* getStateName(int state);
void setCurrentState(const int state);

void transitionTo(const int state);
void transitionToAndThen(const int state, const int nextState);

void setCurrentOrientation();
bool orientationMatches(const Orientation& target, int x, int y, int z);

void processOrientationMatch(uint16_t x, uint16_t y, uint16_t z);
void processOrientationMismatch();
void submitAndPossiblyCompletePhase(uint8_t deviceId);

void setPlayerSubmission(uint8_t deviceId);
bool allPlayersSubmitted();
void processAllPlayersSubmitted();
void resetPlayerSubmissions();

void completePhase();
void completeTransmit();

void playPhaseCompletionEffects(int completedPhase);
void playTransmitCompletionEffects();

bool isCalibrated();

// Returns true if all phases are completed
bool isCalibrated() {
  for (int i = 0; i < NUM_PHASES; i++) {
    if (!phaseCompleted[i]) {
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  delay(2000);

  setupESPNow();
  setupDisplay();
  setupMPU();

  transitionTo(STATE_BOOTING);
  transitionTo(STATE_OFFSETS_SETUP);

  calculateOffsets();

  setupButtons();
  setupEffects();

#ifdef DEVICE_ROLE_MASTER
  transitionTo(STATE_PHASE_STAGED);
#endif
#ifdef DEVICE_ROLE_SLAVE
  transitionTo(STATE_SLAVE_WAITING);
#endif
}

void loop() {
  mpu.update();

  if (currentState != STATE_PROCESSING) {
    return;  // Skip processing if we are in a non-processing state
  }

  if ((millis() - orientationRefreshTimer) > ORIENTATION_REFRESH_INTERVAL_MS) {
    setCurrentOrientation();
    OLEDController::renderOrientationValues(oled, currentOrientation.x, currentOrientation.y,
                                            currentOrientation.z);

    orientationRefreshTimer = millis();
  }
}

void setupESPNow() {
  espNowHelper.begin(DEVICE_ID);

#ifdef DEVICE_ROLE_MASTER
  Serial.println("Device role: MASTER");
  espNowHelper.addPeer(hubAddress);
  espNowHelper.addPeer(orientationSlave1Address);
  espNowHelper.sendModuleConnected(hubAddress);

  espNowHelper.registerOrientationMessageHandler(&handleSubmissionMessageFromSlave);
#endif

#ifdef DEVICE_ROLE_SLAVE
  Serial.println("Device role: SLAVE");
  espNowHelper.addPeer(orientiationMasterAddress);

  espNowHelper.registerOrientationPhaseMessageHandler(&handlePhaseMessageFromMaster);
  espNowHelper.registerOrientationTransmissionHandler(&handleTransmissionMessageFromMaster);
#endif
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

  Serial.println("  ✓ MPU6050 initialized.");
}

void setupButtons() {
  Button* resetOffsetsButton = new Button(RESET_OFFSETS_BUTTON_PIN, false);
  resetOffsetsButton->attachPressDownEventCb(&handleOffsetsButtonPressed, NULL);

  Button* submitButton = new Button(SUBMIT_PHASE_BUTTON_PIN, false);
  submitButton->attachPressDownEventCb(&handleSubmitPhaseButtonPressed, NULL);

#ifdef DEVICE_ROLE_MASTER
  Serial.println("Setting up master load phase button...");
  Button* masterLoadPhaseButton = new Button(LOAD_PHASE_BUTTON_PIN, false);
  masterLoadPhaseButton->attachPressDownEventCb(&handleLoadPhaseButtonPressed, NULL);

  Serial.println("Setting up master transmit button...");
  Button* masterTransmitButton = new Button(TRANSMIT_BUTTON_PIN, false);
  masterTransmitButton->attachPressDownEventCb(&handleTransmitButtonPressed, NULL);
#endif
}

void setupEffects() {
#ifdef DEVICE_ROLE_MASTER
  pinMode(LED_PHASE_1_SUCCESS_PIN, OUTPUT);
  pinMode(LED_PHASE_2_SUCCESS_PIN, OUTPUT);
  pinMode(LED_PHASE_3_SUCCESS_PIN, OUTPUT);
  pinMode(LED_TRANSMITTED_PIN, OUTPUT);

  digitalWrite(LED_PHASE_1_SUCCESS_PIN, LOW);
  digitalWrite(LED_PHASE_2_SUCCESS_PIN, LOW);
  digitalWrite(LED_PHASE_3_SUCCESS_PIN, LOW);
  digitalWrite(LED_TRANSMITTED_PIN, LOW);

  digitalWrite(BUZZER_PIN, LOW);
#endif
}

void calculateOffsets() {
  Serial.println("Calculating MPU6050 offsets, do not move MPU6050");
  // delay(1000);
  mpu.calcOffsets(CALCULATE_OFFSET_GYRO, CALCULATE_OFFSET_ACCEL);
  delay(1000);
}

void handleOffsetsButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Offsets button pressed");

  if (currentState != STATE_PROCESSING) {
    return;
  }

  int oldState = currentState;
  transitionTo(STATE_OFFSETS_SETUP);

  calculateOffsets();              // compute new bias offsets at current position first
  setupMPU();                      // re-init: seeds angleX/Y from accelerometer using new offsets
  angleZOffset = mpu.getAngleZ();  // angleZ is gyro-only and never reset by begin() -- snapshot it

  transitionTo(oldState);
}

void handleSubmitPhaseButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Submit phase button pressed");

  if (currentState != STATE_PROCESSING) {
    return;
  }

  if (currentPhase >= NUM_PHASES) {
    return;
  }

  int x = currentOrientation.x;
  int y = currentOrientation.y;
  int z = currentOrientation.z;
  Serial.printf("Current orientation: x=%d, y=%d, z=%d\n", x, y, z);

  const Orientation& target = phaseTargets[currentPhase];
  if (orientationMatches(target, x, y, z)) {
    processOrientationMatch(x, y, z);
  } else {
    processOrientationMismatch();
  }
}

void handleLoadPhaseButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Master load phase button pressed");

  if (currentState != STATE_PHASE_STAGED) {
    return;
  }

  espNowHelper.sendOrientationPhaseUpdated(orientationSlave1Address, currentPhase);
  transitionToAndThen(STATE_PHASE_LOADING, STATE_PROCESSING);
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

// Slave -> Master: Slave submitted orientation match for current phase
void handleSubmissionMessageFromSlave(const OrientationSubmissionMessage& message) {
  Serial.printf("Received orientation message from slave module: %d\n", message.deviceId);
  Serial.printf("  Roll: %d, Pitch: %d, Yaw: %d\n", message.roll, message.pitch, message.yaw);

  submitAndPossiblyCompletePhase(message.deviceId);
}

// Master -> Slave: Master started new phase
void handlePhaseMessageFromMaster(const OrientationPhaseMessage& message) {
  Serial.printf("Received orientation progress message from master: %d%%\n", message.phase);

  currentPhase = message.phase;

  transitionToAndThen(STATE_PHASE_LOADING, STATE_PROCESSING);
}

// Master -> Slave: Master transmitted final orientation submission to hub
void handleTransmissionMessageFromMaster(const OrientationTransmissionMessage& message) {
  Serial.println("Received orientation transmission message from master");

  transitionTo(STATE_TRANSMIT_COMPLETE);
}

const char* getStateName(int state) {
  switch (state) {
    case STATE_BOOTING:
      return "STATE_BOOTING";
    case STATE_OFFSETS_SETUP:
      return "STATE_OFFSETS_SETUP";
    case STATE_PHASE_STAGED:
      return "STATE_PHASE_STAGED";
    case STATE_PHASE_LOADING:
      return "STATE_PHASE_LOADING";
    case STATE_PROCESSING:
      return "STATE_PROCESSING";
    case STATE_MASTER_WAITING:
      return "STATE_MASTER_WAITING";
    case STATE_SLAVE_WAITING:
      return "STATE_SLAVE_WAITING";
    case STATE_TRANSMIT_STAGED:
      return "STATE_TRANSMIT_STAGED";
    case STATE_TRANSMIT_COMPLETE:
      return "STATE_TRANSMIT_COMPLETE";
    case STATE_INVALID_SUBMISSION:
      return "STATE_INVALID_SUBMISSION";
    default:
      return "Unknown State";
  }
}

void setCurrentState(const int state) {
  Serial.println("-----------------------------------");
  Serial.printf("➤ ➤ Transitioning to state: (%d) %s\n ", state, getStateName(state));
  Serial.println("-----------------------------------");
  currentState = state;
}

void transitionTo(const int state) {
  switch (state) {
    case STATE_BOOTING:
      setCurrentState(STATE_BOOTING);
      OLEDController::renderBootScreen(oled, COUNTDOWN_SECONDS_BOOT);
      break;
    case STATE_OFFSETS_SETUP:
      setCurrentState(STATE_OFFSETS_SETUP);
      OLEDController::renderOffsetsSetup(oled);
      break;
    case STATE_PHASE_STAGED:
      setCurrentState(STATE_PHASE_STAGED);
      OLEDController::renderPhaseStaged(oled, currentPhase, NUM_PHASES);
      break;
    case STATE_PHASE_LOADING:
      setCurrentState(STATE_PHASE_LOADING);
      OLEDController::renderPhaseLoading(oled, currentPhase, COUNTDOWN_SECONDS_PHASE_START);
      break;
    case STATE_PROCESSING:
      setCurrentState(STATE_PROCESSING);
      OLEDController::renderOrientationLayout(oled);
      break;
    case STATE_MASTER_WAITING:
      setCurrentState(STATE_MASTER_WAITING);
      OLEDController::renderMasterWaitScreen(oled);
      break;
    case STATE_SLAVE_WAITING:
      setCurrentState(STATE_SLAVE_WAITING);
      OLEDController::renderSlaveWaitScreen(oled);
      break;
    case STATE_TRANSMIT_STAGED:
      setCurrentState(STATE_TRANSMIT_STAGED);
      OLEDController::renderTransmitStaged(oled);
      break;
    case STATE_TRANSMIT_COMPLETE:
      setCurrentState(STATE_TRANSMIT_COMPLETE);
      OLEDController::renderTransmitComplete(oled);
      break;
    case STATE_INVALID_SUBMISSION:
      setCurrentState(STATE_INVALID_SUBMISSION);
      OLEDController::renderInvalidSubmissionScreen(oled, COUNTDOWN_SECONDS_INVALID_SUBMISSION);
      break;
    default:
      Serial.printf("✗ Unknown state transition requested: %d\n", state);
  }
}

void transitionToAndThen(const int state, const int nextState) {
  transitionTo(state);
  transitionTo(nextState);
}

void setCurrentOrientation() {
  currentOrientation.x = (int)mpu.getAngleX() * -1;
  currentOrientation.y = (int)mpu.getAngleY();
  currentOrientation.z = (int)(mpu.getAngleZ() - angleZOffset) * -1;
}

bool orientationMatches(const Orientation& target, int x, int y, int z) {
  return abs(target.x - x) <= ORIENTATION_TOLERANCE && abs(target.y - y) <= ORIENTATION_TOLERANCE &&
         abs(target.z - z) <= ORIENTATION_TOLERANCE;
}

void processOrientationMatch(uint16_t x, uint16_t y, uint16_t z) {
#ifdef DEVICE_ROLE_MASTER
  transitionTo(STATE_MASTER_WAITING);

  submitAndPossiblyCompletePhase(DEVICE_ID);
#endif
#ifdef DEVICE_ROLE_SLAVE
  espNowHelper.sendOrientationSubmission(orientiationMasterAddress, x, y, z, currentPhase, true);
  transitionTo(STATE_SLAVE_WAITING);
#endif
}

void processOrientationMismatch() {
  Serial.printf("Phase %d not matched. Try again.\n", currentPhase + 1);
  transitionToAndThen(STATE_INVALID_SUBMISSION, STATE_PROCESSING);
}

void submitAndPossiblyCompletePhase(uint8_t deviceId) {
  setPlayerSubmission(deviceId);

  if (allPlayersSubmitted()) {
    processAllPlayersSubmitted();
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

void processAllPlayersSubmitted() {
  Serial.println("All players submitted successfully for this phase!");
  completePhase();

  if (currentPhase < NUM_PHASES) {
    transitionTo(STATE_PHASE_STAGED);
  } else {
    transitionTo(STATE_TRANSMIT_STAGED);
  }
}

void completePhase() {
  int completedPhase = currentPhase;

  resetPlayerSubmissions();
  phaseCompleted[currentPhase] = true;
  currentPhase++;

  playPhaseCompletionEffects(completedPhase);
}

void completeTransmit() {
  transitionTo(STATE_TRANSMIT_COMPLETE);

  espNowHelper.sendModuleUpdated(hubAddress, true);
  espNowHelper.sendOrientationTransmission(orientationSlave1Address, true);

  playTransmitCompletionEffects();
}

void playPhaseCompletionEffects(int phase) {
  switch (phase) {
    case 0:
      digitalWrite(LED_PHASE_1_SUCCESS_PIN, HIGH);
      break;
    case 1:
      digitalWrite(LED_PHASE_2_SUCCESS_PIN, HIGH);
      break;
    case 2:
      digitalWrite(LED_PHASE_3_SUCCESS_PIN, HIGH);
      break;
    default:
      break;
  }
  BuzzerController::playSuccessMelody();
}

void playTransmitCompletionEffects() {
  digitalWrite(LED_TRANSMITTED_PIN, HIGH);

  BuzzerController::playTriumphMelody();
}
