#include <TM1637Display.h>
 
// Rotary Encoder Inputs
#define CLK2 2
#define DT2 3
#define SW 4
 
// Define the display connection pins:
#define CLK 6
#define DIO 5
 
// Create display object of type TM1637Display:
TM1637Display OurDisplay = TM1637Display(CLK, DIO);
 
// Create array that turns all segments on:
const uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
 
// Create array that turns all segments off:
const uint8_t blank[] = {0x00, 0x00, 0x00, 0x00};
 
const long REPEAT_MILLIS = 500;
long lastClickMillis;

const uint8_t GLYPHS = 28;
const uint8_t GLYPHS_MINUS_ONE = GLYPHS - 1;
const uint8_t SPACE_GLYPH = GLYPHS - 2;
const uint8_t glyphs[GLYPHS] = {
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,         // A
  SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                 // b
  SEG_D | SEG_E | SEG_G,                                 // c
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                 // d
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                 // E
  SEG_A | SEG_E | SEG_F | SEG_G,                         // F
  SEG_A | SEG_B | SEG_D | SEG_E | SEG_F | SEG_G,         // g
  SEG_C | SEG_E | SEG_F | SEG_G,                         // h
  SEG_C,                                                 // i
  SEG_B | SEG_C | SEG_D,                                 // j
  SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,                 // k
  SEG_D | SEG_E | SEG_F,                                 // L
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,                 // M
  SEG_C | SEG_E | SEG_G,                                 // n
  SEG_C | SEG_D | SEG_E | SEG_G,                         // o
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,                 // P
  SEG_A | SEG_B | SEG_C | SEG_F | SEG_G,                 // q
  SEG_E | SEG_G,                                         // r
  SEG_A | SEG_C | SEG_D  | SEG_F,                        // S
  SEG_D | SEG_E | SEG_F | SEG_G,                         // t
  SEG_C | SEG_D | SEG_E,                                 // u
  SEG_D | SEG_E,                                         // v
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,                 // W
  SEG_C | SEG_E,                                         // x
  SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,                 // y
  SEG_A | SEG_B | SEG_D | SEG_E,                         // Z
  0x00,                                                  // space
  SEG_B | SEG_C | SEG_G                                  // end
};

int currentStateCLK;
int lastStateCLK;
const uint8_t MAX_GLYPH_NUMBERS = 64;
uint8_t glyphNumbers[MAX_GLYPH_NUMBERS];
uint8_t numGlyphNumbers = 0;
uint8_t message;
int buzzer = 10;  // pin 10 drives the buzzer
int toneFreq = 0;
 
uint8_t glyphNumber = 0;
uint8_t previousGlyphNumber = 0;
const uint8_t oneOff[] = {0x00};
const int RIGHT_DIGIT = 3;
const int LEFT_DIGIT = 0;
const uint8_t DIGITS = 4;

char *currentDir;

bool scrolling = false; 

void setup() {
 
  // Set encoder pins as inputs
  pinMode(CLK2,INPUT);
  pinMode(DT2,INPUT);
  pinMode(SW,INPUT);
  // Setup Serial Monitor
  Serial.begin(9600);
 
  // Read the initial state of A (CLK)
  lastStateCLK = digitalRead(CLK2);

  lastClickMillis = millis();
 
  // Clear the display:
  OurDisplay.clear();
  delay(1000);
  OurDisplay.setBrightness(7);
  OurDisplay.setSegments(glyphs + glyphNumber, 1, RIGHT_DIGIT);

  currentDir = "-";

  // Call Interrupt Service Routine (ISR) updateEncoder() when any high/low change
  // is seen on A (CLK2) interrupt  (pin 2), or B (DT2) interrupt (pin 3)
 
  attachInterrupt(digitalPinToInterrupt(CLK2), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT2), updateEncoder, CHANGE);

  // There are no more pins with interrupts on an UNO, so we just have to
  // use digitalRead to monitor the switch.
  // Need to software REPEAT_MILLIS. Once a press is 
  // detected and acted on, wait until REPEAT_MILLIS before acting again
}
 
// Given an array of glyph numbers, the numbers to display and the start digit
// Clears and updates
void showGlyphs(uint8_t *glyphNumbers,uint8_t number, uint8_t startDigit) {
uint8_t toDisplay[DIGITS];
  if (startDigit < 0 || startDigit > DIGITS - 1) { // an unsigned value could never be less than zero!
    Serial.print("startDigit out of range ");
    Serial.println(startDigit);
  }
  if (number < 1 || number > DIGITS) {
    Serial.print("number out of range ");
    Serial.println(number);
  }
  for (int i = 0; i < number; i++) {
    toDisplay[i] = glyphs[glyphNumbers[i]];
  }
  OurDisplay.clear();
  OurDisplay.setSegments(toDisplay, number, startDigit);
}

long lastScrollMillis;
int scrollPosition;     // offset of first glyph from leftmost display digit.
                        // We want to message to start off the display to the right
                        // and scroll until the last glyph is on the leftmost
                        // display digit, then repeat
                        // Digits are numbered left to right 0 to 3
                        // Decreases from DIGITS (ie 4) to 1 - numGlyphNumbers
const long scrollDelayMillis = 500;

void loop() {
  long nowMillis = millis();
  if (scrolling) {
    Serial.print("---- scrolling -----");
    if (nowMillis - lastScrollMillis > scrollDelayMillis) {
      scrollPosition--;
      if (scrollPosition == -numGlyphNumbers) {
        scrollPosition = DIGITS;
        OurDisplay.clear();
      } else {
        int startGlyphIndex = max(0,-scrollPosition);
        int limit = min(DIGITS,numGlyphNumbers -startGlyphIndex); // first limit on number of glyphs to display
        Serial.print("limit ");
        Serial.println(limit);
        Serial.print("scrollPosition ");
        Serial.println(scrollPosition);
        Serial.print("start digit ");
        Serial.println(max(0,scrollPosition));
        Serial.print("start glyph ");
        Serial.println(startGlyphIndex);
        Serial.print("number of digits ");
        Serial.println(min(limit,DIGITS - scrollPosition));
        
        showGlyphs(glyphNumbers + startGlyphIndex, min(limit,DIGITS - scrollPosition), max(0,scrollPosition));
      }
      lastScrollMillis = nowMillis;        
    }
  } else { // no scrolling, so must be entering
    if (previousGlyphNumber != glyphNumber) {
      OurDisplay.setSegments(oneOff, 1, RIGHT_DIGIT);
      OurDisplay.setSegments(glyphs + glyphNumber, 1, RIGHT_DIGIT);
      previousGlyphNumber = glyphNumber;
    } // if
    if (digitalRead(SW) == LOW) {
      Serial.println("Button pressed");
      if (nowMillis - lastClickMillis > REPEAT_MILLIS) {
        if (glyphNumber == GLYPHS_MINUS_ONE) { // the 'end' glyph
          scrolling = true;
          scrollPosition = DIGITS; // start position is one beyond the left-most digit
          Serial.println("Switching to scrolling");
          OurDisplay.clear();
          lastScrollMillis = millis();
        } else { // glyphNumber != GLYPHS_MINUS_ONE
          if(numGlyphNumbers != MAX_GLYPH_NUMBERS) {
            glyphNumbers[numGlyphNumbers++] = glyphNumber;
            //glyphNumber = SPACE_GLYPH;
            uint8_t glyphsToShow = min(DIGITS - 1,numGlyphNumbers);
            showGlyphs(glyphNumbers + max(0,numGlyphNumbers - (DIGITS - 1)),glyphsToShow, DIGITS - 1 - min(glyphsToShow, DIGITS - 1));
            OurDisplay.setSegments(glyphs + glyphNumber, 1, RIGHT_DIGIT);
            Serial.println("Added glyph");
          } // if
          lastClickMillis = nowMillis;
        } // else
      } else {
        Serial.println("waiting for repeat");
      } // else
    }
    delay(1);
  } // else
} // loop
 
//  This is our ISR which has the job of responding to interrupt events
//
void updateEncoder(){
  // Read the current state of CLK
  currentStateCLK = digitalRead(CLK2);
 
  // If last and current state of CLK are different, then a pulse occurred;
  // React to only 0->1 state change to avoid double counting
  if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){
 
    // If the DT state is different than the CLK state then
    // the encoder is rotating CW so INCREASE glyphNumber by 1
    if (digitalRead(DT2) != currentStateCLK) {
      glyphNumber = (glyphNumber == GLYPHS_MINUS_ONE) ? 0 : glyphNumber + 1;
      currentDir ="CW";

    } else {
      // Encoder is rotating CCW so DECREASE glyphNumber by 1
      glyphNumber = (glyphNumber == 0) ? GLYPHS_MINUS_ONE : glyphNumber - 1;
      currentDir ="CCW";
    }
 
    //Serial.print("Direction: ");
    //Serial.print(currentDir);
    //Serial.print(" | glyphNumber= ");
    //Serial.println(glyphNumber);
  }
 
  // Remember last CLK state to use on next interrupt...
  lastStateCLK = currentStateCLK;
}