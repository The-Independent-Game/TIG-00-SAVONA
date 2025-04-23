#include <U8x8lib.h>
#include <Wire.h>

#define PIN_SPEAKER 12

typedef struct { 
  int pin;
  int tone;
  byte ledSignal;
} button;

const button buttons[] {
    {2, 261, (byte)0b11111110}, //Y
    {3, 277, (byte)0b11111101}, //G
    {4, 294, (byte)0b11111011}, //B
    {5, 311, (byte)0b11110111}  //R
};

byte animationButton;

byte buttonPressStates;
byte buttonReadyStates;


unsigned long timerPlaying;

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE); 	      

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

  resetButtonStates();

  stopLeds();

  delay(100);
}

void loop() {
  readButtons();

  if (isButtonPressed(0)) {
    u8x8.clear();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"YELLOW");
    ledOn(0, true);
  }

  if (isButtonPressed(1)) {
    u8x8.clear();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"GREEN");
    ledOn(1, true);
  }

  if (isButtonPressed(2)) {
    u8x8.clear();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"BLUE");
    ledOn(2, true);
  }

  if (isButtonPressed(3)) {
    u8x8.clear();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"RED");
    ledOn(3, true);
  }

  if (playingPassed()) {
    stopLeds();
    u8x8.clear();
  }
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

bool playingPassed() {
  return millis() - timerPlaying >= 500;
}

void playingStart() {
  timerPlaying = millis();
}

void ledOn(int ledIndex, bool executeSound){
  button b = buttons[ledIndex];
  Wire.beginTransmission(0x20);
  Wire.write(b.ledSignal);
  Wire.endTransmission();
  if (executeSound) {
    tone(PIN_SPEAKER, b.tone);
  }
  playingStart();
}

void stopLeds() {
  Wire.beginTransmission(0x20);
  Wire.write((byte)0b11111111);
  Wire.endTransmission();
  noTone(PIN_SPEAKER);
}