#include <Arduino.h>
#include <Wire.h>
#define PJON_MAX_PACKETS 4
#define PJON_PACKET_MAX_LENGTH 52
#include <PJONSoftwareBitBang.h>
#include <ButtonDebounce.h>
#include <arduino-timer.h>
#include "../../Escape Room v2 Master/src/tracks.h"

void sendMp3(int track);
void sendLcd(const char *line1, const char *line2);
void send(const char *msg, int len);
void send(uint8_t *msg, uint8_t len);

/* -------------------- GAME STATE ---------------------------*/
boolean isGameOver = false;
#define PIN_SWITCH1     2
#define PIN_SWITCH2     3
#define PIN_SWITCH3     4
#define PIN_SWITCH4     5
#define PIN_SWITCH5     6
#define PIN_SWITCH6     7
#define PIN_CASE        8
#define PIN_POWER_LIGHT 9
#define PIN_CAMERA      10
#define PIN_COMM        13

// case is first supplied power
#define INITIAL 0
// This is not case power, but the power state for the game
#define POWER_OFF 1
#define POWER_ON 2
// aka Tones module
#define MODEM 3
// aka Mastermind Module
#define FIREWALL 4
// aka Keypad module
#define CONTROL_ROOM 5
// aka Blackbox module
#define REACTOR_CORE 6

#define POWER_SWITCHES 0
#define MODEM_SWITCHES 1
#define FIREWALL_SWITCHES 2
#define CONTROL_ROOM_SWITCHES 3
#define REACTOR_CORE_SWITCHES 4

uint8_t GAMES[5] = {POWER_ON, MODEM, FIREWALL, CONTROL_ROOM, REACTOR_CORE};
const char *gameNames[5] = {"Power", "Modem", "Firewall", "Control room",
                            "Reactor core"};

uint8_t gameState = INITIAL;
boolean clearToProceedToNextPanel = true;
uint8_t switchState[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
uint8_t switchesGame[5][6];

ButtonDebounce switch1(PIN_SWITCH1, 50);
ButtonDebounce switch2(PIN_SWITCH2, 50);
ButtonDebounce switch3(PIN_SWITCH3, 50);
ButtonDebounce switch4(PIN_SWITCH4, 50);
ButtonDebounce switch5(PIN_SWITCH5, 50);
ButtonDebounce switch6(PIN_SWITCH6, 50);

void reportCurrentSwitches() {
  char line1[17], line2[17];
  line1[0] = 0;
  line2[0] = 0;
  strcat(line1, "Switches");
  for (int s = 0; s < 6; s++) {
    strcat(line2, switchState[s] == HIGH ? "\2" : "\1");
    strcat(line2, " ");
  }
  sendLcd(line1, line2);
}

void checkSwitches() {
//  reportCurrentSwitches();
  if (!clearToProceedToNextPanel) return;
  for (int g = 0; g < 5; g++) {
    int found = g;
    for (int s = 0; s < 6; s++) {
      if (switchesGame[g][s] != switchState[s]) {
        found = -1;
        break;
      }
    }
    if (found != -1) {
      if (gameState == POWER_OFF && found != 0) return;
      if (gameState != found+1) { //ensure current state is the one prev to the current switches
        return;
      }
      if (gameState != POWER_OFF) {
        clearToProceedToNextPanel = false;  //wait for panel to be completed before enabling switches for next panel
      }

      gameState = GAMES[found];
      if (gameState == POWER_ON) {
          digitalWrite(PIN_POWER_LIGHT, HIGH);
      }
//      sendMp3(found + 3);  //+3 because track 3 is the power up track
      send("G", 1);  //progress game state on master
      break;
    }
  }
}

void switch1Pressed(const int state) {
  switchState[0] = state;
  checkSwitches();
}

void switch2Pressed(const int state) {
  switchState[1] = state;
  checkSwitches();
}

void switch3Pressed(const int state) {
  switchState[2] = state;
  checkSwitches();
}

void switch4Pressed(const int state) {
  switchState[3] = state;
  checkSwitches();
}

void switch5Pressed(const int state) {
  switchState[4] = state;
  checkSwitches();
}

void switch6Pressed(const int state) {
  switchState[5] = state;
  checkSwitches();
}

void reportSwitches() {
  char line1[17], line2[17];
  for (int g = 0; g < 5; g++) {
    line1[0] = 0;
    line2[0] = 0;
    strcat(line1, gameNames[g]);
    for (int s = 0; s < 6; s++) {
      strcat(line2, switchesGame[g][s] == HIGH ? "\2" : "\1");
      strcat(line2, " ");
    }
    sendLcd(line1, line2);
    delay(10000);
  }
  line1[0] = 0;
  line2[0] = 0;
  sendLcd(line1, line2);
}

bool checkForDupSwitch(int g) {
  for (int i = 0; i < g; i++) {
    bool found = true;
    for (int s = 0; s < 6; s++) {
      if (switchesGame[i][s] != switchesGame[g][s]) {
        found = false;
        break;
      }
    }
    if (found) return true;
    found = true;
    // check for all off, which is invalid since it is starting state
    for (int s = 0; s < 6; s++) {
      if (switchesGame[i][s] != HIGH) {
        found = false;
        break;
      }
    }
    if (found) return true;
  }
  return false;
}

Timer<1> startupTimer;

bool callReportSwitches(void *t) {
  reportSwitches();
  return false;
}

void initGameState() {
  pinMode(PIN_SWITCH1, INPUT_PULLUP);
  pinMode(PIN_SWITCH2, INPUT_PULLUP);
  pinMode(PIN_SWITCH3, INPUT_PULLUP);
  pinMode(PIN_SWITCH4, INPUT_PULLUP);
  pinMode(PIN_SWITCH5, INPUT_PULLUP);
  pinMode(PIN_SWITCH6, INPUT_PULLUP);
  pinMode(PIN_POWER_LIGHT, OUTPUT);
  digitalWrite(PIN_POWER_LIGHT, LOW);
  for (int g = 0; g < 5; g++) {
    do {
      for (int s = 0; s < 6; s++) {
        switchesGame[g][s] = random(2);
      }
    } while (checkForDupSwitch(g));
  }
  switch1.setCallback(switch1Pressed);
  switch2.setCallback(switch2Pressed);
  switch3.setCallback(switch3Pressed);
  switch4.setCallback(switch4Pressed);
  switch5.setCallback(switch5Pressed);
  switch6.setCallback(switch6Pressed);
  startupTimer.in(8000*3 + 500*32 + 1000, callReportSwitches);
}

/* -----------------END GAME STATE ----------------------------*/

/* ------------------- CASE ----------------------------------*/

ButtonDebounce caseSwitch(PIN_CASE, 500);
bool introPlayed = false;

void caseOpenClose(const int state) {
  if (gameState == INITIAL) {
    if (state == LOW) {
      // case was closed
      gameState = POWER_OFF;
      digitalWrite(PIN_CAMERA, HIGH); //start camera
    } else {
      // ignore and wait for case to close
    }
  } else {
    if (state == LOW) {
      // tried closing case while playing
      sendMp3(TRACK_COUNTDOWN_STILL_RUNNING);
    } else {  //case opened
      if (gameState == POWER_OFF && !introPlayed) {
        introPlayed = true;
        send("G",1); //Let master know - go to next state
      }
    }
  }
}

void initCase() {
  pinMode(PIN_CASE, INPUT_PULLUP);
  pinMode(PIN_CAMERA, OUTPUT);
  digitalWrite(PIN_CAMERA, LOW);
  caseSwitch.setCallback(caseOpenClose);
}
/* -----------------------End Case ------------------------ */

PJON<SoftwareBitBang> bus(10);

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection with device ID ");
    Serial.print(bus.packets[data].content[0], DEC);
    Serial.println(" is lost.");
  }
  else if(code == PJON_PACKETS_BUFFER_FULL) {
    Serial.print("Packet buffer is full, has now a length of ");
    Serial.println(data);
    Serial.println("Possible wrong bus configuration!");
    Serial.println("higher PJON_MAX_PACKETS if necessary.");
  }
  else if(code == PJON_CONTENT_TOO_LONG) {
    Serial.print("Content is too long, length: ");
    Serial.println(data);
  } else {
    Serial.println("PJON error");
    Serial.println(code);
    Serial.println(data);
  }
}

void commReceive(uint8_t *data, uint16_t len, const PJON_Packet_Info &info) {
  switch(data[0]) {
    case 'C':
      clearToProceedToNextPanel = true;
      break;
    default:
      break;
  }
}

void sendLcd(const char *line1, const char *line2) {
  uint8_t msg[35];
  msg[0] = 'L';
  strncpy((char *)&msg[1], line1, 17);
  strncpy((char *)&msg[18], line2, 17);
  send(msg, 35);
}

void sendMp3(int track) {
  uint8_t msg[2];
  msg[0] = 'M';
  msg[1] = track;
  send(msg, 2);
}

void sendTone(int tone) {
  uint8_t msg[2];
  msg[0] = 'T';
  msg[1] = tone;
  send(msg, 2);
}

void send(const char *msg, int len) {
  uint8_t buf[35];
  memcpy(buf, msg, len);
  send(buf, len);
}

void send(uint8_t *msg, uint8_t len) {
  bus.send(1, msg, len);
  while (bus.update()) {};//wait for send to be completed
}

void initComm() {
  bus.strategy.set_pin(PIN_COMM);
  bus.set_error(error_handler);
  bus.set_receiver(commReceive);
  bus.begin();
}

void startup() {
  digitalWrite(PIN_POWER_LIGHT, HIGH);
  delay(1000);
  digitalWrite(PIN_POWER_LIGHT, LOW);
}

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(9600);
  delay(2000);  //let Master start first
  // put your setup code here, to run once:
  initComm();
  initCase();
  initGameState();
  startup();
}

void loop() {
  caseSwitch.update();
  switch1.update();
  switch2.update();
  switch3.update();
  switch4.update();
  switch5.update();
  switch6.update();
  startupTimer.tick();
  bus.update();
  bus.receive(750);  //try to receive for .75 ms
}