 /*
   PROJECT "SCHAUKASTEN"
   ColorCube "Farbwürfel" Prototype Version 1 - Proof of concept
   Created 27 May 2017 FG


   User Interactions
   -----------------
   When pushing the cube (on the touch of the built-in button), ColorCube reads the colour that is underneath it and displays it using Neopixels.

   push cube
 * * longer than 60 seconds --> FACTORY RESET
 * * longer than 10 seconds on top of black cardboard --> CALIBRATE BLACK
 * * longer than 10 seconds on top of white cardboard --> CALIBRATE WHITE
 * * longer than 4 seconds on red --> CHANGE LED PAPPTERN (normal --> blinking --> 'round trip')
 * * longer than 10 seconds on top of green cardboard --> SAVE CHANGES


*/

#include <EEPROM.h>
int address = 0;

// Neopixel related
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#define PIN            11 // PIN For NEO Pixels
#define NUMPIXELS      5 // We use 5 Neopixels in this proof-of-concept
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
int pixelCounter = 0;
boolean showAllPixels = true;

// Colour sensor related
#define S0 4 // not really neccessary. can also solder S0 to 5V
#define S1 5 // not really neccessary. can also solder S0 to GND
#define S2 6
#define S3 7
#define sensorOut 8
#define powerPin 10
#define colorTaster 3 // or sometimes 2 in some cubes I built
int frequency = 0;
int r = 0;
int g = 0;
int b = 0;
int rRaw = 0;
int gRaw = 0;
int bRaw = 0;
const int nReadingsArray = 15;
int rRawAvgArray[nReadingsArray];
int gRawAvgArray[nReadingsArray];
int bRawAvgArray[nReadingsArray];
int rRawAvg = 0;
int gRawAvg = 0;
int bRawAvg = 0;
int rRawMax = 200;  // note, maximum raw sensor reading is equivalent to "color is not present"
int gRawMax = 200;  // --> Max value: read with black card
int bRawMax = 160;
int rRawMin = 50;  // --> Min value: read with white card
int gRawMin = 50;
int bRawMin = 50;


//
int old_r = 0;
int old_g = 0;
int old_b = 0;

boolean buttonState = LOW;
boolean buttonIsReleased = true;
boolean buttonIsPressed = false;
boolean bootUpComplete = false;

long timeStamp = 0; // timer - how long was the button pressed?


// Variables for implementing blinking etc.
long timeStampBlink = 0; // timer for implementing blinking
long lastPressed = 0;
boolean isPaused = false;
boolean isLEDOn = true;
boolean isBlinkModeOff = true; // set this variable true to set cube to blink mode
boolean turnOffCube = false;
int displayMode = 0; // 0 == normal mode    1 == blink mode   2 == 'round trip mode'    3 == 'pulsating mode'

// Calibration Procedure
unsigned int calibrateCube = 0;
#define BLACK 5
#define WHITE 4
#define RED 3
#define GREEN 2
#define BLUE 1



// SETUP
// ==================================================================================
void setup() {
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  pinMode(powerPin, OUTPUT); 
  digitalWrite(powerPin, LOW);
  pinMode(3, INPUT);
  digitalWrite(3, HIGH);
  pinMode(A0, OUTPUT);  // A0 is for the Auto-shutoff Circuit of Cube V3. It is not used in V2beta, but it won't do any harm
  digitalWrite(A0, HIGH);

  pinMode(colorTaster, INPUT); //  Button Pin
  digitalWrite(colorTaster, HIGH);

  // Setting frequency-scaling to 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  Serial.begin(9600);
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // End of trinket special code

  loadSettings();

  pixels.begin(); // This initializes the NeoPixel library.

  timeStampBlink = millis();
  Serial.println("setup complete");
  lastPressed = millis();
}



// LOOP
// ==================================================================================
void loop() {
  // for Cuebe V3
  if (abs(millis()-lastPressed)>60000) {
    digitalWrite(A0, LOW);
  }
  
  buttonState = digitalRead(colorTaster); // does the user push ColorCube?
  // If button is pressed
  if (buttonState == LOW) {
    digitalWrite(powerPin, HIGH);

    buttonIsPressed = true;
    lastPressed = millis();
    //timeStampBlink = millis(); // this line stops the cube from blinking if the button is pressed
    // If button was not pressed before, capture timestamp
    if (buttonIsReleased == true)
      timeStamp = millis();
    buttonIsReleased = false;

    if (calibrateCube == BLACK) {
      //giveFeedback(30, 30, 30, 1);
      rRawMax = rRawAvg * 0.5; // the colour mapping looks better (stronger colours) if we cut off the maximum value --> every value bigger than a certain threshold becomes full colour
      gRawMax = gRawAvg * 0.5;
      bRawMax = bRawAvg * 0.45;
      Serial.print("RMay: ");
      Serial.print(rRawMax);
      Serial.print(" ");
      Serial.print("GMax: ");
      Serial.print(gRawMax);
      Serial.print(" ");
      Serial.print("BMax: ");
      Serial.print(bRawMax);
      Serial.println(" ");
      calibrateCube = 0;
      saveSettings();
    }
    if (calibrateCube == WHITE) {
      //giveFeedback(255, 255, 255, 1);
      rRawMin = rRawAvg;
      gRawMin = gRawAvg;
      bRawMin = bRawAvg;
      Serial.print("RMin: ");
      Serial.print(rRawMin);
      Serial.print(" ");
      Serial.print("GMin: ");
      Serial.print(gRawMin);
      Serial.print(" ");
      Serial.print("BMin: ");
      Serial.print(bRawMin);
      Serial.println(" ");
      calibrateCube = 0;
      saveSettings();
    }

    // read color sensor

    // einmalige Pause
    if (!bootUpComplete) {
      bootUpComplete = true;
      delay(150);
    }
    
    readAvgRawColors();
    mapColors();

  }  // end of if button is pressed
  else {
    bootUpComplete = false;

    buttonIsPressed = false;
    digitalWrite(powerPin, LOW);
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);

    // button was pressed an now it has been released
    if (buttonIsReleased == false) {
      //Serial.println(abs(millis() - timeStamp));
      // If button was pressed longer than n milliseconds
      // I extended by a long long time instead of deleting this obsolete procedure
      Serial.println(abs(millis() - timeStamp));
      if (abs(millis() - timeStamp) > 10000) {
        //Serial.println("here");

        // Save Settings
        //if (rRawAvg < 120 && gRawAvg < 120 & bRawAvg > 70) {
        if (rRawAvg < 70 && gRawAvg > 105 & bRawAvg < 90) {
          giveFeedback(0, 255, 0, 3);
          saveSettings();
        }

        // If a black card or object is presented: calibrate black
        //if (r == 0 && g == 0 & b == 0) {
        //if (rRawAvg > 200 && gRawAvg > 200 & bRawAvg > 160) {
        if (rRawAvg > 150 && gRawAvg > 170 && bRawAvg > 130) {  
          giveFeedback(255, 0, 0, 0);
          giveFeedback(0, 255, 0, 0);
          giveFeedback(0, 0, 255, 0);
          calibrateCube = BLACK;
          Serial.println("calibrate black");
        }
        // If a white card or object is presented: calibrate white
        //if (r == 255 && g == 255 & b == 255) {
        if (rRawAvg < 50 && gRawAvg < 50 & bRawAvg < 50) {
          giveFeedback(255, 0, 0, 0);
          giveFeedback(0, 255, 0, 0);
          giveFeedback(0, 0, 255, 0);
          calibrateCube = WHITE;
        }
      }

      if (abs(millis() - timeStamp) > 4000) {
        //giveFeedback(0, 255, 0, 2);
        if (rRawAvg < 70 && gRawAvg > 50 & bRawAvg > 50) {
          giveFeedback(255, 0, 0, 1);
          // turn blinkmode on
          displayMode = displayMode + 1;
          if (displayMode > 2)
            displayMode = 0;
        }
      }

      if (abs(millis() - timeStamp) > 60000) {
        factoryReset();
      }
    }

    buttonIsReleased = true;
  }


  // The following code takes care of the cube's colouring
  // -----------------------------------------------------
  prepareDisplay();
  // helper code-segment for implementing blink mode
  if (abs(millis() - timeStampBlink) > 500) {
    timeStampBlink = millis();
    if (pixelCounter > NUMPIXELS)
      pixelCounter = -1;
    pixelCounter = pixelCounter + 1;
    toggleLED();
  }

  if (showAllPixels) { // each surface of the cube is illuminated
    turnOffCube = true;
    // Turn on LEDs, either in normal or in Blink Mode
    if (isLEDOn || isBlinkModeOff || buttonIsPressed) {
      turnOffCube = false;
      for (int i = 0; i < NUMPIXELS; i++) {
        // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
        pixels.setPixelColor(i, pixels.Color(old_r, old_g, old_b)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
      }
    }
  } else { // illuminate one side of the cube at the time
    turnOffCube = true;
    // Turn on LEDs, either in normal or in Blink Mode

    if (buttonIsPressed) {
      turnOffCube = false;
      for (int i = 0; i < NUMPIXELS; i++) {
        // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
        pixels.setPixelColor(i, pixels.Color(old_r, old_g, old_b)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
      }
    } else {
      if (isLEDOn) {
        turnOffCube = false;
        pixels.setPixelColor(pixelCounter, pixels.Color(old_r, old_g, old_b)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
      }
    }
  }

  if (turnOffCube) {
    pixels.clear();
    pixels.show();
  }

} // end of loop


// HELPER FUNCTIONS
// ==================================================================================
void toggleLED() {
  if (isLEDOn == true) {
    isLEDOn = false;
  } else {
    isLEDOn = true;
  }
}

// makes the cube blink n times in colours r g b
void giveFeedback(int r, int g, int b, int n) {
  for (int j = 0; j <= n; j++) {
    for (int i = 0; i < NUMPIXELS; i++) {
      // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, pixels.Color(r, g, b)); // Moderately bright green color.
      pixels.show(); // This sends the updated pixel color to the hardware.
    }
    delay(150);
    pixels.clear();
    pixels.show();
    delay(150);
  }
}


// read aerage raw colors
void readAvgRawColors() {
  Serial.println("read average colors");
  int rSum = 0; int gSum = 0; int bSum = 0;;
  for (int i = 0; i < nReadingsArray; i++) {
    //Serial.println(i);
    // Reading the output frequency
    // Setting red filtered photodiodes to be read
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
    frequency = pulseIn(sensorOut, LOW);
    rRaw = frequency;
    rRawAvgArray[i] = rRaw;
    rSum = rSum + rRaw;
    //delay(10);
    // Setting Green filtered photodiodes to be read
    digitalWrite(S2, HIGH);
    digitalWrite(S3, HIGH);
    // Reading the output frequency
    frequency = pulseIn(sensorOut, LOW);
    gRaw = frequency;
    gRawAvgArray[i] = gRaw;
    gSum = gSum + gRaw;
    //delay(10);
    // Setting Blue filtered photodiodes to be read
    digitalWrite(S2, LOW);
    digitalWrite(S3, HIGH);
    // Reading the output frequency
    frequency = pulseIn(sensorOut, LOW);
    bRaw = frequency;
    bRawAvgArray[i] = bRaw;
    bSum = bSum + bRaw;
  }
  rRawAvg = rSum / nReadingsArray;
  gRawAvg = gSum / nReadingsArray;
  bRawAvg = bSum / nReadingsArray;
  Serial.print("R_Raw_Avg= ");//printing name
  Serial.print(rRawAvg);//printing RED color frequency
  Serial.print("  ");
  Serial.print("G_Raw_Avg= ");//printing name
  Serial.print(gRawAvg);//printing RED color frequency
  Serial.print("  ");
  Serial.print("B_Raw_Avg= ");//printing name
  Serial.print(bRawAvg);//printing RED color frequency
  Serial.println("  ");
  //delay(100);
}

/*
  // read raw colors
  void readRawColors() {
  // Reading the output frequency
  // Setting red filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  frequency = pulseIn(sensorOut, LOW);
  rRaw = frequency;
  Serial.print("R_Raw= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.print("  ");
  delay(100);
  // Setting Green filtered photodiodes to be read
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(sensorOut, LOW);
  gRaw = frequency;
  // Printing the value on the serial monitor
  Serial.print("G_Raw= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.print("  ");
  delay(100);
  // Setting Blue filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(sensorOut, LOW);
  bRaw = frequency;
  // Printing the value on the serial monitor
  Serial.print("B_Raw= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.println("  ");
  delay(40);
  }*/


// maps the average raw colours to values [0;255]
void mapColors() {
  frequency = rRawAvg;
  if (frequency > rRawMax)
    frequency = rRawMax;
  if (frequency < rRawMin)
    frequency = rRawMin;
  frequency = map(frequency, rRawMin, rRawMax, 255, 0);
  old_r = r;
  r = frequency;
  // Printing the value on the serial monitor
  /*Serial.print("R= ");//printing name
    Serial.print(frequency);//printing RED color frequency
    Serial.print("  ");
    delay(100);
  */
  frequency = gRawAvg;
  if (frequency > gRawMax)
    frequency = gRawMax;
  if (frequency < gRawMin)
    frequency = gRawMin;
  frequency = map(frequency, gRawMin, gRawMax, 255, 0);
  old_g = g;
  g = frequency;
  // Printing the value on the serial monitor
  /*Serial.print("G= ");//printing name
    Serial.print(frequency);//printing RED color frequency
    Serial.print("  ");
    delay(100);*/
  frequency = bRawAvg;
  //    Remaping the value of the frequency to the RGB Model of 0 to 255
  if (frequency > bRawMax)
    frequency = bRawMax;
  if (frequency < bRawMin)
    frequency = bRawMin;
  frequency = map(frequency, bRawMin, bRawMax, 255, 0);
  old_b = b;
  b = frequency;
  // Printing the value on the serial monitor
  /*Serial.print("B= ");//printing name
    Serial.print(frequency);//printing RED color frequency
    Serial.println("  ");
    delay(40);*/
  Serial.print("RMapped: ");
  Serial.print(r);
  Serial.print(" ");
  Serial.print("Gmapped: ");
  Serial.print(g);
  Serial.print(" ");
  Serial.print("Bmapped: ");
  Serial.print(b);
  Serial.println(" ");                                                                                      
}





// takes a sensor reading
// the result is assigned to golbal variables r, g, and b
void readColors() {


  // Reading the output frequency
  // Setting red filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  //Serial.println(digitalRead(3));
  frequency = pulseIn(sensorOut, LOW);
  //Remaping the value of the frequency to the RGB Model of 0 to 255


  if (frequency > 330)
    frequency = 330;
  if (frequency < 100)
    frequency = 100;
  frequency = map(frequency, 100, 330, 255, 0);
  old_r = r;
  r = frequency;
  // Printing the value on the serial monitor
  Serial.print("R= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.print("  ");
  delay(50);
  // Setting Green filtered photodiodes to be read
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(sensorOut, LOW);
  //Remaping the value of the frequency to the RGB Model of 0 to 255

  if (frequency > 500)
    frequency = 500;
  if (frequency < 100)
    frequency = 100;
  frequency = map(frequency, 100, 500, 255, 0);
  old_g = g;
  g = frequency;
  // Printing the value on the serial monitor
  Serial.print("G= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.print("  ");
  delay(50);
  // Setting Blue filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(sensorOut, LOW);
  //    Remaping the value of the frequency to the RGB Model of 0 to 255

  if (frequency > 330)
    frequency = 330;
  if (frequency < 30)
    frequency = 30;
  frequency = map(frequency, 30, 330, 255, 0);
  old_b = b;
  b = frequency;
  // Printing the value on the serial monitor
  Serial.print("B= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.println("  ");
  delay(40);
}


// save all settings to EEPROM
/*
   display mode: normal, blinking, etc.
   displayMode

   rRawMax
   gRawMax
   bRawMax
   rRawMin
   gRawMin
   bRawMin
*/
void saveSettings() {
  address = 0;
  EEPROM.write(address, displayMode);
  address = address + 1;
  EEPROM.write(address, rRawMax);
  address = address + 1;
  EEPROM.write(address, gRawMax);
  address = address + 1;
  EEPROM.write(address, bRawMax);
  address = address + 1;
  EEPROM.write(address, rRawMin);
  address = address + 1;
  EEPROM.write(address, gRawMin);
  address = address + 1;
  EEPROM.write(address, bRawMin);
  address = 0;
  giveFeedback(0, 0, 255, 1);
}

void loadSettings() {
  address = 0;
  displayMode  = EEPROM.read(address);
  address = address + 1;
  rRawMax = EEPROM.read(address);
  address = address + 1;
  gRawMax = EEPROM.read(address);
  address = address + 1;
  bRawMax = EEPROM.read(address);
  address = address + 1;
  rRawMin = EEPROM.read(address);
  address = address + 1;
  gRawMin = EEPROM.read(address);
  address = address + 1;
  bRawMin = EEPROM.read(address);
  address = 0;
}


void prepareDisplay() {
  switch (displayMode) {
    case 0:
      isBlinkModeOff = true;
      showAllPixels = true;
      break;
    case 1:
      isBlinkModeOff = false;
      showAllPixels = true;
      break;
    case 2:
      showAllPixels = false;
      isBlinkModeOff = true;
      break;
  }
}

void factoryReset() {
  rRawAvg = 0;
  gRawAvg = 0;
  bRawAvg = 0;
  rRawMax = 200;  // note, maximum raw sensor reading is equivalent to "color is not present"
  gRawMax = 200;  // --> Max value: read with black card
  bRawMax = 160;
  rRawMin = 50;  // --> Min value: read with white card
  gRawMin = 50;
  bRawMin = 50;
  displayMode = 0;
  saveSettings();
}


