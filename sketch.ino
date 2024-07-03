#include <EEPROM.h>
#include <U8x8lib.h>
#include <Wire.h>

#define PIN_SPEAKER 12
#define NO_BUTTON 255

typedef struct { 
  int pin;
  int tone;
  byte ledSignal;
} button;

enum gameStates {
                  LOBBY,  
                  SEQUENCE_CREATE_UPDATE,
                  SEQUENCE_PRESENTING,
                  PLAYER_WAITING,
                  GAME_OVER,
                  OPTIONS,
                  OPTIONS_ASK_RESET
                };


const button buttons[] {
    {2, 261, (byte)0b11111110}, //Y
    {3, 277, (byte)0b11111101}, //G
    {4, 294, (byte)0b11111011}, //B
    {5, 311, (byte)0b11110111}  //RED
};

int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};
//            mid C  C#   D    D#   E    F    F#   G    G#   A
byte level;
byte gameSequence[25];  //100 is the maximum level *TODO: remove*
gameStates gameState;

byte animationButton;

byte presentingIndex;
byte playerPlayingIndex;

byte buttonPressStates;
byte buttonReadyStates;

bool needWait;

unsigned long timerPlaying, timerPause, timerPlayerWaiting;

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE); 	      

uint8_t record;


void setup() {
  Wire.begin();

  for(int i = 0; i < sizeof(buttons)/sizeof(button); ++i) {
    pinMode(buttons[i].pin, INPUT);
  }

  Serial.begin(9600);

  randomSeed(analogRead(0));

  u8x8.begin();
  u8x8.setPowerSave(0);

  delay(100);

  Serial.println("setup OK");

  changeGameState(LOBBY);

  delay(100);
}

void loop() {
  
  readButtons();
  
  // if (areAllButtonPressed() && gameState != OPTIONS) {
  //   changeGameState(OPTIONS);
  // }

  switch (gameState) {
    case OPTIONS:
      if (isButtonPressed(3)) { //exit
        changeGameState(LOBBY);
      }
      if (isButtonPressed(0)) { //reset ?
        changeGameState(OPTIONS_ASK_RESET);
      }
      break;
    case OPTIONS_ASK_RESET:
      if (isButtonPressed(2)) {
        record = 0;
        EEPROM.write(0,record);
        changeGameState(OPTIONS);
      } else if (isButtonPressed(3)) {
        changeGameState(OPTIONS);
      }
      break;
    case LOBBY:
      if (random(0,400000) == 0) {
        allOn();
        tone(PIN_SPEAKER, tones[random(0,sizeof(tones)/sizeof(int))]);
        delay(500);
        noTone(PIN_SPEAKER);
        stopLeds();
      } else {  
        if (playingPassed()) {
          rotateAnimation();
        }
      }
      if (buttonPressStates) {
        allOn();
        delay(1500);
        stopLeds();
        delay(500);
        changeGameState(SEQUENCE_CREATE_UPDATE);
      }
      break;
    case SEQUENCE_CREATE_UPDATE:
      Serial.print("level ");
      Serial.print(level);
      Serial.print(" - ");
      Serial.println(500 - penalty(500) + 100);

      for (int n = level - 1; n < sizeof(gameSequence)/sizeof(byte); n++) {
        if (n < level) {
          gameSequence[n] = randomButton();
        } else {
          gameSequence[n] = NO_BUTTON; //clear prev sequence
        }
      }
      changeGameState(SEQUENCE_PRESENTING);
      break;
    case SEQUENCE_PRESENTING:
      if (playingPassed() && !needWait) {
        if (pausePassed()) {
          presentingIndex ++;
          int currentButton = gameSequence[presentingIndex];
          if (currentButton != NO_BUTTON) {
            ledOn(currentButton, true);
            needWait = true;
          } else {
            changeGameState(PLAYER_WAITING);
            playerWaitingStart();
          }
        }
      } else if (needWait && playingPassed()) {
        pauseStart();
        stopLeds();
        needWait = false;
      }
      break;
    case PLAYER_WAITING:
      if (playerWaitingTimeOut()) {
        Serial.println("Player TIMEOUT");
        playerTimeOutEffect();
        delay(1000);
        changeGameState(GAME_OVER);
      } else {
        if (playingPassed() || buttonPressStates) {
          stopLeds();
          if (gameSequence[playerPlayingIndex] == NO_BUTTON) {
            Serial.println("New Level");
            delay(500);
            level ++;
            changeGameState(SEQUENCE_CREATE_UPDATE);
          } else {
            bool buttonPressedFound = false;
            int len = sizeof(buttons)/sizeof(button);
            int i = 0;
            while (!buttonPressedFound && i < len) {
              if (isButtonPressed(i)) {
                if (gameSequence[playerPlayingIndex] == i) {
                  playerWaitingStart();
                  ledOn(i,true);
                  playerPlayingIndex ++;
                  buttonPressedFound = true;
                } else {
                  changeGameState(GAME_OVER);
                }
              }  
              i ++;
            }
          }
        }
      }
      break;
    case GAME_OVER:
      Serial.println("AAAAAA");
      gameOver();
     changeGameState(LOBBY);
      break;
  }
}

byte randomButton() {
  return (byte) random(0, 4);
}

bool isButtonPressed(byte button) {
  return bitRead(buttonPressStates, button) == 1;
}

bool areAllButtonPressed() {
  int count = 0;
  for (int i = 0; i < sizeof(buttons)/sizeof(button); i++) {
    if (digitalRead(buttons[i].pin)) count ++;
  }
  return count == sizeof(buttons)/sizeof(button);
}

void readButtons() {
  for (int i = 0; i < sizeof(buttons)/sizeof(button); i++) {
    if (digitalRead(buttons[i].pin)) {
      if (bitRead(buttonReadyStates, i)) {
        bitClear(buttonReadyStates, i);
        buttonPressStates = bitSet(buttonPressStates, i);
      } else {
        buttonPressStates = bitClear(buttonPressStates, i);
      }
    } else {
      bitSet(buttonReadyStates, i);
      buttonPressStates = bitClear(buttonPressStates, i);
    }
  }
}

void rotateAnimation() {
  int transTable[4] = {0,1,3,2};
  if (++animationButton > 3) animationButton = 0;
  ledOn(transTable[animationButton], false);
}

byte penalty (int base) {
  double difficulty = (double)-1/((double)level * (double)level) + 0.5;
  int penalty = 0;
  if (difficulty > 0) {
    penalty += (byte) floor(difficulty * base);
  }
  return penalty;
}

bool playerWaitingTimeOut() {
  return millis() - timerPlayerWaiting >= 5000;
}

bool playingPassed() {
  return millis() - timerPlaying >= 500 - penalty(400);
}

bool pausePassed() {
  return millis() - timerPause >= 300 - penalty(200);
}

void pauseStart() {
  timerPause = millis();
}

void playingStart() {
  timerPlaying = millis();
}

void playerWaitingStart() {
  timerPlayerWaiting = millis();
}

void changeGameState(gameStates newState) {
  Serial.print("from ");
  Serial.print(gameState);
  Serial.print(" to ");
  Serial.println(newState);
  gameState = newState;
  switch (gameState) {
    case OPTIONS_ASK_RESET:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"CONFIRM ?");
      u8x8.drawString(0,1,"B-YES R-NO");
      break;
    case OPTIONS:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"OPTIONS");
      u8x8.drawString(0,1,"B-YES R-NO");
      break;
    case LOBBY:
      record = EEPROM.read(0);
      level = 1;
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"TIG-00");
      u8x8.drawString(0,1,"Press a button");
      break;
    case SEQUENCE_PRESENTING:
      presentingIndex = -1;
      needWait = false;
      break;
    case SEQUENCE_CREATE_UPDATE:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,(((String) "Level  ") + level).c_str ());
      u8x8.drawString(0,1,(((String) "Record ") + record).c_str () );
      break;
    case PLAYER_WAITING:
      playerPlayingIndex = 0;
      break;
    case GAME_OVER:
      if (level > record) {
        record = level;
        EEPROM.write(0,record);
      }
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"GAME OVER !");
      break;
  }
}

void ledOn(int ledIndex, bool sound){
  button b = buttons[ledIndex];
  Wire.beginTransmission(0x20);
  Wire.write(b.ledSignal);
  Wire.endTransmission();
  if (sound) {
    tone(PIN_SPEAKER, b.tone);
  }
  playingStart();
}

void allOn() {
  Wire.beginTransmission(0x20);
  Wire.write((byte)0b11110000);
  Wire.endTransmission();
}

void playerTimeOutEffect() {
  allOn();
  tone(PIN_SPEAKER, tones[4]);
}

void startMatchEffect() {
  allOn();
}

void stopLeds() {
  Wire.beginTransmission(0x20);
  Wire.write((byte)0b11111111);
  Wire.endTransmission();
  noTone(PIN_SPEAKER);
}

void gameOver() {

  byte melody[] = { 250, 196, 196, 220, 196,0, 247, 250};

  byte noteDurations[] = {4, 8, 8, 4, 4, 4, 4, 4 };

  int noteDuration = 0;
  int pauseBetweenNotes = 0;

 for (byte thisNote = 0; thisNote < sizeof(melody)/sizeof(byte); thisNote++) {
    noteDuration = 1000/noteDurations[thisNote];
    tone(PIN_SPEAKER, melody[thisNote],noteDuration);
    if (melody[thisNote] > 0) {
      rotateAnimation();
    }
    pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(PIN_SPEAKER);
    if (melody[thisNote] > 0) {
      stopLeds();
    }
  }
}