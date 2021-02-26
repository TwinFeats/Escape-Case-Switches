#include <Arduino.h>
#include <Wire.h>
#define PJON_MAX_PACKETS 2
#define PJON_PACKET_MAX_LENGTH 33
#include <PJONSoftwareBitBang.h>
#include <ButtonDebounce.h>
#include "../../Escape Room v2 Master/src/tracks.h"
#include <NeoPixelBrightnessBus.h>

void sendMp3(int track);
void sendLcd(char *line1, char *line2);
void send(uint8_t *msg, uint8_t len);

/* -------------------- GAME STATE ---------------------------*/
boolean isGameOver = false;
#define CASE 1
#define SWITCH1 2
#define SWITCH2 3
#define SWITCH3 4
#define SWITCH4 5
#define SWITCH5 6
#define SWITCH6 7
#define POWER_LIGHT 8

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

uint8_t gameState = POWER_OFF;
boolean clearToProceedToNextPanel = true;
uint8_t switchState[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
uint8_t switchesGame[5][6];

NeoPixelBus<NeoRgbFeature, Neo400KbpsMethod> powerLight(1, POWER_LIGHT);

ButtonDebounce switch1(SWITCH1, 100);
ButtonDebounce switch2(SWITCH2, 100);
ButtonDebounce switch3(SWITCH3, 100);
ButtonDebounce switch4(SWITCH4, 100);
ButtonDebounce switch5(SWITCH5, 100);
ButtonDebounce switch6(SWITCH6, 100);

RgbColor green(0,255,0);

void checkSwitches() {
  for (int g = 0; g < 5; g++) {
    int found = -1;
    for (int s = 0; s < 6; s++) {
      if (switchesGame[g][s] != switchState[s]) {
        found = g;
      }
      break;
    }
    if (found != -1) {
      if (gameState == POWER_OFF && found != 0) return;
      if (!clearToProceedToNextPanel || gameState != found - 1) { //ensure current state is the one prev to the current switches
        sendMp3(TRACK_FUNCTION_INACCESSIBLE);
        return;
      }
      if (gameState != POWER_OFF) {
        clearToProceedToNextPanel = false;  //wait for panel to be completed before enabling switches for next panel
      }
      if (gameState == POWER_ON) {
        powerLight.SetPixelColor(0, green);
        powerLight.Show();
      }

      gameState = GAMES[found];
      sendMp3(found + 3);  //+3 because track 3 is the power up track
      send((uint8_t *)"G", 1);  //progress game state on master
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
  line1[0] = 0;
  line2[0] = 0;
  for (int g = 0; g < 5; g++) {
    strcat(line1, gameNames[g]);
    for (int s = 0; s < 6; s++) {
      strcat(line2, switchesGame[g][s] == HIGH ? "\2" : "\1");
      strcat(line2, " ");
    }
    sendLcd(line1, line2);
    delay(10000);
  }
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

void initGameState() {
  pinMode(SWITCH1, INPUT_PULLUP);
  pinMode(SWITCH2, INPUT_PULLUP);
  pinMode(SWITCH3, INPUT_PULLUP);
  pinMode(SWITCH4, INPUT_PULLUP);
  pinMode(SWITCH5, INPUT_PULLUP);
  pinMode(SWITCH6, INPUT_PULLUP);
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
  powerLight.Begin();
  powerLight.Show();
}

/* -----------------END GAME STATE ----------------------------*/

/* ------------------- CASE ----------------------------------*/
#define caseOpen 49

ButtonDebounce caseSwitch(caseOpen, 100);

void caseOpenClose(const int state) {
  if (gameState == INITIAL) {
    if (state == LOW) {
      // case was closed
      gameState = POWER_OFF;
      send((uint8_t *)"G",1);
    } else {
      // this shouldn't happen as case is open when connected to power, so
      // ignore and wait for case to close
    }
  } else {
    if (state == LOW) {
      // tried closing case while playing
      sendMp3(TRACK_COUNTDOWN_STILL_RUNNING);
    } else {
      if (gameState == POWER_OFF) {
        sendMp3(TRACK_INTRO);
      }
    }
  }
}

void initCase() {
   caseSwitch.setCallback(caseOpenClose);
}
/* -----------------------End Case ------------------------ */

PJON<SoftwareBitBang> bus(10);

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection lost with device id ");
    Serial.println(bus.packets[data].content[0], DEC);
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

void sendLcd(char *line1, char *line2) {
  uint8_t msg[33];
  msg[0] = 'L';
  strncpy((char *)&msg[1], line1, 16);
  strncpy((char *)&msg[17], line2, 16);
  send(msg, 33);
}

void sendMp3(int track) {
  uint8_t msg[2];
  msg[0] = 'M';
  msg[1] = track;
  send(msg, 2);
}

void send(uint8_t *msg, uint8_t len) {
  bus.send(1, msg, len);
  bus.update();
}

void initComm() {
  bus.strategy.set_pin(13);
  bus.include_sender_info(false);
  bus.set_error(error_handler);
  bus.set_receiver(commReceive);
  bus.begin();
}

void setup() {
  delay(2000);  //let Master start first
  // put your setup code here, to run once:
  initComm();
  initCase();
  initGameState();
}

void loop() {
  // put your main code here, to run repeatedly:
  bus.update();
  bus.receive(750);  //try to receive for .75 ms
}