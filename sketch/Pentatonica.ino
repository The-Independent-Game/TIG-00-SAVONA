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
                  OPTIONS_ASK_RESET,
                  OPTIONS_ASK_SOUND,
                  INSERT_NAME
                };

// AGGIORNATO: Toni pentatonici per migliore esperienza audio
const button buttons[] {
    {2, 262, (byte)0b11111110}, //Y - Do (C4)
    {3, 294, (byte)0b11111101}, //G - Re (D4)  
    {4, 330, (byte)0b11111011}, //B - Mi (E4)
    {5, 392, (byte)0b11110111}  //R - Sol (G4)
};

// AGGIORNATO: Scala pentatonica estesa per effetti musicali
int tones[] = {262, 294, 330, 392, 440, 494, 523, 587, 659, 698};
//            C4   D4   E4   G4   A4   B4   C5   D5   E5   F5
// Pentatonica base: Do-Re-Mi-Sol-La + estensioni armoniche

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

bool sound;

byte nameIndex;
char nameLetter;
String recordName;

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

  Serial.println("setup OK - Pentatonic Edition");

  changeGameState(LOBBY);

  sound = true;

  delay(100);
}

void loop() {
  
  //consuming button reading
  readButtons();

  //lowlevel button reading
  if (areAllButtonPressed() && gameState != OPTIONS) {
    resetButtonStates();
    changeGameState(OPTIONS);
  }

  switch (gameState) {
    case OPTIONS:
      if (isButtonPressed(3)) { //exit
        changeGameState(LOBBY);
      }
      if (isButtonPressed(2)) { //reset ?
        changeGameState(OPTIONS_ASK_RESET);
      }
      if (isButtonPressed(0)) { //sound ?
        changeGameState(OPTIONS_ASK_SOUND);
      }
      break;
    case OPTIONS_ASK_RESET:
      if (isButtonPressed(2)) {
        record = 0;
        EEPROM.write(0,record);
        EEPROM.write(1, 0);
        EEPROM.write(2, 0);
        EEPROM.write(3, 0);
        changeGameState(OPTIONS);
      } else if (isButtonPressed(3)) {
        changeGameState(OPTIONS);
      }
      break;
    case OPTIONS_ASK_SOUND:
      if (isButtonPressed(2)) {
        sound = true;
        changeGameState(OPTIONS);
      } else if (isButtonPressed(3)) {
        sound = false;
        changeGameState(OPTIONS);
      }
      break;
    case LOBBY:
      if (random(0,400000) == 0) {
        allOn();
        if (sound) tone(PIN_SPEAKER, tones[random(0,sizeof(tones)/sizeof(int))]);
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
            // AGGIORNATO: Melody di avanzamento livello più armoniosa
            if (sound) levelUpMelody();
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
                  if (level > record) {
                    record = level;
                    nameLetter = 'A';
                    recordName = "";
                    u8x8.clear();
                    u8x8.setFont(u8x8_font_chroma48medium8_r);
                    u8x8.drawString(0,0,"!! NEW RECORD !!");
                    endGameMelody();
                    changeGameState(INSERT_NAME);
                    rewriteName();
                  } else {
                    changeGameState(GAME_OVER);
                  }
                }
              }  
              i ++;
            }
          }
        }
      }
      break;
    case GAME_OVER:
      endGameMelody();
      changeGameState(LOBBY);
      break;
    case INSERT_NAME:
      if (isButtonPressed(0)) { //BACK
        if (recordName.length() > 0) {
            recordName.remove(recordName.length() - 1); 
        }
        rewriteName();
      } else if (isButtonPressed(1)) { //prev LETTER
        if (nameLetter > 'A') nameLetter --;
        rewriteName();
      } else if (isButtonPressed(2)) { //CONFIRM
        if (recordName.length() < 3) {
          recordName += nameLetter;
          rewriteName();
        } else {
          saveRecord();
          changeGameState(LOBBY);
        }
      } else if (isButtonPressed(3)) { //next LETTER
        if (nameLetter < 'Z') nameLetter ++;
        rewriteName();
      }
      break;
  }
}

void saveRecord() {
  EEPROM.write(0,record);
  EEPROM.write(1,recordName.charAt(0));
  EEPROM.write(2,recordName.charAt(1));
  EEPROM.write(3,recordName.charAt(2));
}

byte randomButton() {
  return (byte) random(0, 4);
}

bool isButtonPressed(byte button) {
  return bitRead(buttonPressStates, button) == 1;
}

void resetButtonStates() {
  for (int i = 0; i < sizeof(buttons)/sizeof(button); i++) {
    //ready
    bitSet(buttonReadyStates, i);
    //not pressed
    buttonPressStates = bitClear(buttonPressStates, i);
  }
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

void rewriteName() {
  u8x8.clear();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0,0,"INSERT NAME");
  u8x8.drawString(0,1,recordName.c_str());
  u8x8.drawString(0,2,String(nameLetter).c_str());
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
      u8x8.drawString(0,0,"RESET RECORD ?");
      u8x8.drawString(0,1,"B-YES R-NO");
      break;
    case OPTIONS_ASK_SOUND:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"SOUND ?");
      u8x8.drawString(0,1,"B-YES R-NO");
      break;
    case OPTIONS:
      noTone(PIN_SPEAKER);
      stopLeds();
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"OPTIONS");
      u8x8.drawString(0,1,"B-RESET RECORD");
      u8x8.drawString(0,2,"Y-SOUND");
      u8x8.drawString(0,3,"R-EXIT");
      delay(2000);
      break;
    case LOBBY:
      record = EEPROM.read(0);
      recordName = "";
      recordName += (char) EEPROM.read(1);
      recordName += (char) EEPROM.read(2);
      recordName += (char) EEPROM.read(3);
      level = 1;
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"TIG-00 ♪");
      u8x8.drawString(0,2,"Press a button");
      if (record > 0) {
        u8x8.drawString(0,4,(((String) "Record ") + record).c_str () );
        u8x8.drawString(0,5,(((String) "By ") + recordName).c_str () );
      }
      break;
    case SEQUENCE_PRESENTING:
      presentingIndex = -1;
      needWait = false;
      break;
    case SEQUENCE_CREATE_UPDATE:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,(((String) "Level  ") + level).c_str ());
      if (record > 0) {
        u8x8.drawString(0,2,(((String) "Record ") + record).c_str () );
        u8x8.drawString(0,3,(((String) "By ") + recordName).c_str () );
      }
      break;
    case PLAYER_WAITING:
      playerPlayingIndex = 0;
      break;
    case GAME_OVER:
      u8x8.clear();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"GAME OVER !");
      break;
    default:
      break;
  }
}

void ledOn(int ledIndex, bool executeSound){
  button b = buttons[ledIndex];
  Wire.beginTransmission(0x20);
  Wire.write(b.ledSignal);
  Wire.endTransmission();
  if (executeSound) {
    if (sound) tone(PIN_SPEAKER, b.tone);
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
  // AGGIORNATO: Suono di timeout più drammatico in scala pentatonica
  if (sound) tone(PIN_SPEAKER, 196); // Sol bemolle (tritono diabolico!)
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

// NUOVA FUNZIONE: Melody di avanzamento livello pentatonica
void levelUpMelody() {
  // Arpeggio ascendente pentatonico: Do-Mi-Sol-Do alto
  int levelMelody[] = {262, 330, 392, 523};
  int durations[] = {150, 150, 150, 300};
  
  for (int i = 0; i < 4; i++) {
    if (sound) tone(PIN_SPEAKER, levelMelody[i]);
    delay(durations[i]);
    noTone(PIN_SPEAKER);
    delay(50);
  }
}

// AGGIORNATA: Melody finale più armoniosa con scala pentatonica
void endGameMelody() {
  // Sequenza pentatonica discendente più melodiosa
  int melody[] = {523, 440, 392, 330, 294, 262, 220, 196};
  int noteDurations[] = {4, 6, 4, 6, 4, 4, 4, 2};

  int noteDuration = 0;
  int pauseBetweenNotes = 0;

  for (byte thisNote = 0; thisNote < sizeof(melody)/sizeof(int); thisNote++) {
    noteDuration = 1000/noteDurations[thisNote];
    if (sound) tone(PIN_SPEAKER, melody[thisNote], noteDuration);
    
    // Effetto visivo sincronizzato con la musica
    if (melody[thisNote] > 300) {
      rotateAnimation();
    }
    
    pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(PIN_SPEAKER);
    
    if (melody[thisNote] > 300) {
      stopLeds();
    }
  }
}
