/*
 BeatBag - A Speed Bag Counter
 Nathan Seidle
 SparkFun Electronics
 2/23/2013

 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 BeatBag is a speed bag counter that uses an accelerometer to counts the number hits. 
 It's easily installed ontop of speed bag platform only needing an accelerometer attached to the top of platform. 
 You don't have to alter the hitting surface or change out the swivel.

 I combine X/Y/Z into one vector and look only at the magnitude. 
 I use a fourth order filter to see the impacts (accelerometer peaks) from the speed bag. It works pretty well.
 It's very reproducible but I'm not entirely sure how accurate it is. I can detect both bag hits (forward/backward) then
 I divide by two to get the number displayed to the user.

 I arrived at the peak detection algorithm using video and raw data recordings. After a fourth filtering I could glean the
 peaks. There is probably a much better way to do the math on the peak detection but it's not one of my strength.

 Hardware setup:
 5V from wall supply goes into barrel jack on Redboard. Trace cut to diode.
 RedBoard barel jack is wired to power switch then to Vin diode
 Display gets power from Vin and data from I2C pins
 Vcc/Gnd from RedBoard goes into Bread Board Power supply that supplies 3.3V to accelerometer. Future
 versions should get power from 3.3V rail on RedBoard. 

 MMA8452 Breakout ------------ Arduino
 3.3V --------------------- 3.3V
 SDA(yellow) -------^^(330)^^------- A4
 SCL(blue) -------^^(330)^^------- A5
 GND ---------------------- GND
 The MMA8452 is 3.3V so we recommend using 330 or 1k resistors between a 5V Arduino and the MMA8452 breakout.
 The MMA8452 has built in pull-up resistors for I2C so you do not need additional pull-ups.

 3/2/2013 - Got data from Hugo and myself, 3 rounds, on 2g setting. Very noisy but mostly worked

 12/19/15 - Segment burned out. Power down display after 10 minutes of non-use.
 Use I2C, see if we can avoid the 'multiply by 10' display problem.

 1/23/16 - Accel not reliable. Because the display is now also on the I2C the pull-up resistors on the accel where
 not enough. Swapped out to new accel. Added 100 ohm inline resistors to accel and 4.7k resistors from SDA/SCL to 5V.
 Reinforced connection from accel to RedBoard.


-------------------------------------------------
modified by Marcio Pereira - 30/06/2016
-------------------------------------------------

 */

#include <avr/wdt.h> //We need watch dog for this program

#include <Wire.h> // Used for I2C

#define DISPLAY_ADDRESS 0x71 //I2C address of OpenSegment display

int hitCounter = 0; //Keeps track of the number of hits

const int resetButton = 6; //Button that resets the display and counter
const int LED = 13; //Status LED on D3

long lastPrint; //Used for printing updates every second

boolean displayOn; //Used to track if display is turned off or not

//Used in the new algorithm
float lastMagnitude = 0;
float lastFirstPass = 0;
float lastSecondPass = 0;
float lastThirdPass = 0;
long lastHitTime = 0;
int secondsCounter = 0;

// --------------------------------------------------
// Variable insertion  
float x[2]; // accel
float y[2];
float z[2];
float delta[90]; // delta accel
float delta_now;
float m1[110]; // moving average of delta
float m1_now;
float m2[30]; // moving average of m1
float m2_now;
float finalmean[3]; // test for hit -> peak = hit
//---------------------------------------------------

//This was found using a spreadsheet to view raw data and filter it
const float WEIGHT = 0.9;

//This was found using a spreadsheet to view raw data and filter it
const int MIN_MAGNITUDE_THRESHOLD = 1000; //350 is good

//This is the minimum number of ms between possible hits
//We use this to filter out peaks that are too close together
const int MIN_TIME_BETWEEN_HITS = 90; //100 works well

//This is the number of miliseconds before we turn off the display
long TIME_TO_DISPLAY_OFF = 60L * 1000L * 5L; //5 minutes of no use

int DEFAULT_BRIGHTNESS = 50; //50% brightness to avoid burning out segments after 3 years of use

unsigned long currentTime; //Used for millis checking

void setup()
{
  wdt_reset(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  pinMode(resetButton, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  //By default .begin() will set I2C SCL to Standard Speed mode of 100kHz
  Wire.setClock(400000); //Optional - set I2C SCL to High Speed Mode of 400kHz
  Wire.begin(); //Join the bus as a master

  Serial.begin(115200);
  Serial.println("Speed Bag Counter");

  initDisplay();

  clearDisplay();
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.print("Accl"); //Display an error until accel comes online
  Wire.endTransmission();

  while(!initMMA8452()) //Test and intialize the MMA8452
    ; //Do nothing

  clearDisplay();
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.print("0000");
  Wire.endTransmission();

  lastPrint = millis();
  lastHitTime = millis();

  wdt_enable(WDTO_250MS); //Unleash the beast
}

void loop()
{
  wdt_reset(); //Pet the dog

  currentTime = millis();
  if ((unsigned long)(currentTime - lastPrint) >= 1000)
  {
    if (digitalRead(LED) == LOW)
      digitalWrite(LED, HIGH);
    else
      digitalWrite(LED, LOW);

    lastPrint = millis();
  }

  //See if we should power down the display due to inactivity
  if (displayOn == true)
  {
    currentTime = millis();
    if ((unsigned long)(currentTime - lastHitTime) >= TIME_TO_DISPLAY_OFF)
    {
      Serial.println("Power save");

      hitCounter = 0; //Reset the count

      clearDisplay(); //Clear to save power
      displayOn = false;
    }
  }

  //Check the accelerometer
  //float currentMagnitude = getAccelData();

  //--------------------------------------------------------------
  // My code here
  // recording past
   x[1] = x[2];
   y[1] = y[2];
   z[1] = z[2];
   finalmean[2] =finalmean[3];
   finalmean[1] = finalmean[2];
  // Get the 3 magnitudes separately 
   float x[2] = getAccelData();
   float y[2] = getAccelData();
   float z[2] = getAccelData();
  // instantaneous acceleration delta
   delta_now=abs(x[2]-x[1])+ abs(y[2]-y[1]) + abs(z[2]-z[1]);
   //first moving average
   m1_now = rollingAverage(delta,90,delta_now);
   //second moving average
   m2_now= rollingAverage( m1, 110, m1_now);
   //third moving average
   finalmean[3]=rollingAverage(m2,30, m2_now);
   //Hit test - 80 is lower limit
   if ((finalmean[3]<finalmean[2]) && (finalmean[2]>finalmean[1]) && (finalmean[2]>80))
    {
      hitCounter++; 
      if (displayOn == false) displayOn = true;
      printHits();
    }
 //----------------------------------------------------------------  

/*
  //Send this value through four (yes four) high pass filters
  float firstPass = currentMagnitude - (lastMagnitude * WEIGHT) - (currentMagnitude * (1 - WEIGHT));
  lastMagnitude = currentMagnitude; //Remember this for next time around

  float secondPass = firstPass - (lastFirstPass * WEIGHT) - (firstPass * (1 - WEIGHT));
  lastFirstPass = firstPass; //Remember this for next time around

  float thirdPass = secondPass - (lastSecondPass * WEIGHT) - (secondPass * (1 - WEIGHT));
  lastSecondPass = secondPass; //Remember this for next time around

  float fourthPass = thirdPass - (lastThirdPass * WEIGHT) - (thirdPass * (1 - WEIGHT));
  lastThirdPass = thirdPass; //Remember this for next time around
  //End high pass filtering

  fourthPass = abs(fourthPass); //Get the absolute value of this heavily filtered value

  //See if this magnitude is large enough to care
  if (fourthPass > MIN_MAGNITUDE_THRESHOLD)
  {
    //We have a potential hit!

    currentTime = millis();
    if ((unsigned long)(currentTime - lastHitTime) >= MIN_TIME_BETWEEN_HITS)
    {
      //We really do have a hit!
      hitCounter++;

      lastHitTime = millis();

      //Serial.print("Hit: ");
      //Serial.println(hitCounter);

      if (displayOn == false) displayOn = true;

      printHits(); //Updates the display
    }
  
  */
  }


  //Check if we need to reset the counter and display
  if (digitalRead(resetButton) == LOW)
  {
    //This breaks the file up so we can see where we hit the reset button
    Serial.println();
    Serial.println();
    Serial.println("Reset!");
    Serial.println();
    Serial.println();

    hitCounter = 0;

    resetDisplay(); //Forces cursor to beginning of display
    printHits(); //Updates the display

    while (digitalRead(resetButton) == LOW) wdt_reset(); //Pet the dog while we wait for you to remove finger

    //Do nothing for 250ms after you press the button, a sort of debounce
    for (int x = 0 ; x < 25 ; x++)
    {
      wdt_reset(); //Pet the dog
      delay(10);
    }
  }
}

//This function makes sure the display is at 57600
void initDisplay()
{
  resetDisplay(); //Forces cursor to beginning of display

  printHits(); //Update display with current hit count

  displayOn = true;

  setBrightness(DEFAULT_BRIGHTNESS);
}

//Set brightness of display
void setBrightness(int brightness)
{
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(0x7A); // Brightness control command
  Wire.write(brightness); // Set brightness level: 0% to 100%
  Wire.endTransmission();
}

void resetDisplay()
{
  //Send the reset command to the display - this forces the cursor to return to the beginning of the display
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write('v');
  Wire.endTransmission();

  if (displayOn == false)
  {
    setBrightness(DEFAULT_BRIGHTNESS); //Power up display
    displayOn = true;
    lastHitTime = millis();
  }
}

//Push the current hit counter to the display
void printHits()
{
  int tempCounter = hitCounter / 2; //Cut in half

  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(0x79); //Move cursor
  Wire.write(4); //To right most position

  Wire.write(tempCounter / 1000); //Send the left most digit
  tempCounter %= 1000; //Now remove the left most digit from the number we want to display
  Wire.write(tempCounter / 100);
  tempCounter %= 100;
  Wire.write(tempCounter / 10);
  tempCounter %= 10;
  Wire.write(tempCounter); //Send the right most digit

  Wire.endTransmission(); //Stop I2C transmission
}

//Clear display to save power (a screen saver of sorts)
void clearDisplay()
{
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(0x79); //Move cursor
  Wire.write(4); //To right most position

  Wire.write(' ');
  Wire.write(' ');
  Wire.write(' ');
  Wire.write(' ');

  Wire.endTransmission(); //Stop I2C transmission
}
