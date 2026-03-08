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
const int COUNTDOWN_SECONDS_BOOT = 3;
const int COUNTDOWN_SECONDS_PHASE_START = 5;
const int COUNTDOWN_SECONDS_INVALID_SUBMISSION = 4;

const Orientation phaseTargets[TOTAL_PHASES] = {
    {0, 0, 10},  // Phase 1
    {0, 0, 15},  // Phase 2
    {0, 0, 10}   // Phase 3
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

void handlePhaseMessageFromMaster(const OrientationPhaseMessage& message);
void handleTransmissionMessageFromMaster(const OrientationTransmissionMessage& message);
void handleSubmissionMessageFromSlave(const OrientationSubmissionMessage& message);

void handleMasterOrientationMatched();
void handleSlaveOrientationMatched(int deviceId);

void handleSubmitPhaseButtonPressed(void* button_handle, void* usr_data);
void handleLoadPhaseButtonPressed(void* button_handle, void* usr_data);
void handleTransmitButtonPressed(void* button_handle, void* usr_data);

void completePhase();
void completeTransmit();

void calculateOffsets();

void setCurrentState(const int state);
void transitionTo(const int state);
void transitionToAndThen(const int state, const int nextState);

void submitAndPossiblyCompletePhase(uint8_t deviceId);
void setPlayerSubmission(uint8_t deviceId);
bool allPlayersSubmitted();
void resetPlayerSubmissions();
bool isCalibrated();

void playPhaseCompletionEffects(int completedPhase);
void playTransmitCompletionEffects();

// Returns true if all phases are completed
bool isCalibrated() {
  for (int i = 0; i < TOTAL_PHASES; i++) {
    if (!phaseCompleted[i]) {
      return false;
    }
  }
  return true;
}

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

void setup() {
  Serial.begin(115200);

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

  Wire.begin();
  delay(2000);

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

  setupDisplay();
  setupMPU();

  transitionTo(STATE_BOOTING);
  transitionTo(STATE_OFFSETS_SETUP);

  delay(1000);

  calculateOffsets();

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

  if ((millis() - timer) > ORIENTATION_DISPLAY_INTERVAL_MS) {
    setCurrentOrientation();
    OLEDController::renderOrientation(oled, currentOrientation.x, currentOrientation.y,
                                      currentOrientation.z);

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

  Serial.println("  ✓ MPU6050 initialized.");
}

void calculateOffsets() {
  Serial.println("Calculating MPU6050 offsets, do not move MPU6050");
  mpu.calcOffsets(CALCULATE_OFFSET_GYRO, CALCULATE_OFFSET_ACCEL);
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
      OLEDController::renderPhaseStaged(oled, currentPhase, TOTAL_PHASES);
      break;
    case STATE_PHASE_LOADING:
      setCurrentState(STATE_PHASE_LOADING);
      OLEDController::renderPhaseLoading(oled, currentPhase, COUNTDOWN_SECONDS_PHASE_START);
      break;
    case STATE_PROCESSING:
      setCurrentState(STATE_PROCESSING);
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
  currentOrientation.z = (int)mpu.getAngleZ() * -1;
}

bool orientationMatches(const Orientation& target, int x, int y, int z) {
  return abs(target.x - x) <= ORIENTATION_TOLERANCE && abs(target.y - y) <= ORIENTATION_TOLERANCE &&
         abs(target.z - z) <= ORIENTATION_TOLERANCE;
}

// Slave -> Master: Slave submitted orientation match for current phase
void handleSubmissionMessageFromSlave(const OrientationSubmissionMessage& message) {
  Serial.printf("Received orientation message from slave module: %d\n", message.deviceId);
  Serial.printf("  Roll: %d, Pitch: %d, Yaw: %d\n", message.roll, message.pitch, message.yaw);

  // We are assuming any message from slave is a successful orientation match for the current phase
  // since slave only sends when it matches
  handleSlaveOrientationMatched(message.deviceId);
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
      transitionTo(STATE_TRANSMIT_STAGED);
      return;
    }

    transitionTo(STATE_PHASE_STAGED);
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
    transitionTo(STATE_MASTER_WAITING);
    handleMasterOrientationMatched();
#endif
#ifdef DEVICE_ROLE_SLAVE
    espNowHelper.sendOrientationSubmission(orientiationMasterAddress, x, y, z, currentPhase, true);
    transitionTo(STATE_SLAVE_WAITING);
#endif
  } else {
    Serial.printf("Phase %d not matched. Try again.\n", currentPhase + 1);
    transitionToAndThen(STATE_INVALID_SUBMISSION, STATE_PROCESSING);
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
