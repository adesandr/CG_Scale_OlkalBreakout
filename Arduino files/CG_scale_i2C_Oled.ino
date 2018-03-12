/*************************************************************************************************
 *  CG scale for F3F & F3B models
 *  Original concept by Olav Kallhovd 2016-2017. 
 *  Breakout with a unique Arduino pro and OLED sreen by Alain Désandré,junuary 2018. 
 *  CG Scale main components:
 *    1 pc load sensor front YZC-133 2kg
 *    1 pc load sensor rear YZC-133 3kg
 *    2 pc HX711 ADC, one for each load sensor (128bit resolution)
 *    1 pc Arduino Pro for the scale and the display
 *    1 pc Oled 0.96" ssd1306 or 16*2 HD44780 LCD (see CG_scale_1602_I2C), both using I2C
 *    3D printed parts
 *    Max model weight with sensors above: 4 to 4,5kg depending on CG location
 *
 *  Libraries used 
 *    - HX711_ADC from Olav Kallhovd Github is used
 *    - Adafruit_SSD1306 (or LiquidCristal_I2C depending the screen used)
 *  
 *  alain.desandre@wanadoo.fr - V1.0 - 14/01/2018
 */

/*--- Used to send debug on the serial monitor              ---*/
//#define CG_SCALE_DEBUG

/*--- Delay for the Welcome Display                         ---*/
#define CG_SCALE_DELAY_WELCOME 3000

/*--- Delay for stabilize                                   ---*/
#define CG_SCALE_DELAY_STABILIZE 10000

/*---- Various                                               ---*/
#define TRUE 1
#define FALSE 0

/*--- Includes declaration                                  ---*/
// Librairie Arduino de base
#include <Arduino.h>

//https://github.com/olkal/HX711_ADC can be installed from the library manager
//Number of samples and some filtering settings can be adjusted in the HX711_ADC.h library file
//The best RATE setting is usually 10SPS, see HX711 data sheet (HX711 pin 15, can usually be set by a solder jumper on the HX711 module)
//RATE 80SPS will also work fine, but conversions will be more noisy, so consider increasing number of samples in HX711_ADC.h
#include <HX711_ADC.h> 

// Adafruit_SSD1306 library. This library works with the Adafruit_GFX librarie.
// This library can be installed from the library manager
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// I2C Library.This library is installed by default by the arduino developpement environnement.
#include <Wire.h>

/*--- Global variable declaration                             ---*/

//HX711 constructor (dout pin, sck pint):
HX711_ADC LoadCell_1(A2, A3); //HX711 pins front sensor (DOUT, PD_SCK)
HX711_ADC LoadCell_2(A0, A1); //HX711 pins rear sensor (DOUT, PD_SCK)

// OLED constructor
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

byte ledPin = LED_BUILTIN;    // onboard led. Flash indicates that CG_SCALE is running.
boolean ledState;
byte batRefPin = A7;          // A4 in the original concept, but A4, A5 used on I2C, so we move to A7
char toWeightLCD[8];          // Three buffer used to manage the display
char toCgLCD[9];
char toBatLCD[11];
boolean output;
long t1;
long t2;
boolean bBatWarning = FALSE;

const int printInterval = 500; // LCD/Serial refresh interval

//*** configuration:
//*** set dimensional calibration values:
const long WingPegDist = 1214; //calibration value in 1/10mm, projected distance between wing support points, measure with calliper (old : 1198)
const long LEstopperDist = 306; //calibration value 1/10mm, projected distance from front wing support point to leading edge (stopper pin), measure with calliper (old : 300)
//*** set scale calibration values (best to have the battery connected when doing calibration):
const float ldcell_1_calfactor = 897.0; // user set calibration factor load cell front (float) old : 954
const float ldcell_2_calfactor = 745.0; // user set calibration factor load cell rear (float) old : 799
//***
const long stabilisingtime = 3000; // tare precision can be improved by adding a few seconds of stabilising time
//***
const long CGoffset = ((WingPegDist / 2) + LEstopperDist) * 10;

/********************************************************************
 * void setup(void)
 * 
 * Setup
 ********************************************************************/
void setup() {
  //***
  output = 1; //change to 1 for OLED, output = 0 for Serial terminal (for calibrating), output = 1 for OLED !!!
  //***
  
  // open serial monitor for debug
  Serial.begin(9600);

  if(output == 1) {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  display.display();                          // splash screen display. See Adafruit_SSD1306.cpp, for the memory buffer init.
  }

  // wait for stabilize
  delay(CG_SCALE_DELAY_STABILIZE);

  if(output == 1) { // if output to OLED display
  
  // Welcome message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("F3X COG SCALE - V1.0"));
  display.setCursor(0,21);
  display.println(F("ANEG Custom"));
  display.setCursor(0,44);
  display.println(F("Concept Olav Kallhovd"));
  display.display(); 
  delay(CG_SCALE_DELAY_WELCOME);
  }
  
  if (output == 0) { //if output to serial terminal
    Serial.println();
    Serial.println("Wait for stabilising and tare...");
  }

  /*--- HX711 init. and calibration factor setting ---*/
  LoadCell_1.begin();
  LoadCell_2.begin();
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  while ((loadcell_1_rdy + loadcell_2_rdy) < 2) { //run startup, stabilisation and tare, both modules simultaneously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilisingtime);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilisingtime);
  }
  LoadCell_1.setCalFactor(ldcell_1_calfactor); // set calibration factor
  LoadCell_2.setCalFactor(ldcell_2_calfactor); // set calibration factor

  /*--- Led intialization                          ---*/
  pinMode(ledPin, OUTPUT); //led
  digitalWrite(ledPin, HIGH);

} /* End Setup() */

/******************************************************
 * int readBattVoltage(void)
 * 
 * input : void
 * outpout : int battvalue - battery voltage value
 * 
 ******************************************************/
int readBattVoltage(boolean *bWarn) { // read battery voltage
  long battvalue = 0;
  battvalue += analogRead(batRefPin);
  battvalue += analogRead(batRefPin);
  battvalue *= 4883L; // analog reading * (5.00V*1000000)/1024 (adjust value if VCC is not 5.0V)
  battvalue /= 640L; // this number comes from the resistor divider value ((R2/(R1+R2))*1000)/noof analogreadings (adjust value if required)
  //Serial.println(battvalue);
  if (battvalue < 7500) { //
    *bWarn = TRUE;
  }
  return battvalue;
} /* end readBattVoltage() */

/*******************************************************
 * void flashLed(void)
 * 
 * flash the LEDBUILTIN
 * 
 *******************************************************/
void flashLED() {
  if (t2 < millis()) {
    if (ledState) {
      t2 = millis() + 2000;
      ledState = 0;
    }
    else {
      t2 = millis() + 100;
      ledState = 1;
    }
    digitalWrite(ledPin, ledState);
  }
} /* end flashLED */

/********************************************************
 * void loop(void)
 * 
 * Main loop
 * 
 * ******************************************************/
void loop() {
  
  //library function update() should be called at least as often as HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS
  //longer delay in scetch will reduce effective sample rate (be careful with delay() in loop)
  LoadCell_1.update();
  LoadCell_2.update();

  // calculate CG and update serial/LCD
  if (t1 < millis()) {
    t1 = millis() + printInterval;
    float a = LoadCell_1.getData();
    float b = LoadCell_2.getData();
    long weightAvr[2];
    float CGratio;
    long CG;
    weightAvr[0] = a * 100;
    weightAvr[1] = b * 100;
    long weightTot = weightAvr[0] + weightAvr[1];
  
    if (weightAvr[0] > 500 && weightAvr[1] > 500) {
      long a = weightAvr[1] / 10;
      long b = weightAvr[0] / 10;
      CGratio = (((a * 10000) / (a + b))); 
      CG = ((((WingPegDist) * CGratio) / 1000) - ((WingPegDist * 10) / 2) + CGoffset);
    }
    else {
      CG = 0;
    }
    
    // if output = 0: print result to serial terminal:
    if (output == 0) {
      for (byte a = 0; a < 2; a++) {
        Serial.print(F("weight_LdCell_"));
        Serial.print(a + 1);
        Serial.print(F(": "));
        long i = weightAvr[a];
        if (i < 0) {
          Serial.print('-');
          i = ~weightAvr[a];
        }
        Serial.print(i / 100);
        Serial.print('.');
        if ((i % 100) < 10) {
          Serial.print(F("0"));
        }
        Serial.print(i % 100);
        Serial.print(F("      "));
      }
      Serial.print(F("CG:"));
      Serial.print(CG / 100);
      Serial.print('.');
      Serial.println(CG % 100);
    }
    else { //if output = 1: print to OLED

      if (weightTot < 0 && weightTot >= - 100) {
        weightTot = 0;
      }
      
      if (weightTot < -100) {
        toWeightLCD[0] = 'W';
        toWeightLCD[1] = 't';
        toWeightLCD[2] = ':';
        toWeightLCD[3] = 'E';
        toWeightLCD[4] = 'r';
        toWeightLCD[5] = 'r';
        toWeightLCD[6] = '.';
        toWeightLCD[7] ='\0';
      }
      else {
        toWeightLCD[0] = 'W';
        toWeightLCD[1] = 't';
        toWeightLCD[2] = ':';
        toWeightLCD[3] = (char)((weightTot / 100000)+48);
        toWeightLCD[4] = (char)(((weightTot % 100000) / 10000) + 48);
        toWeightLCD[5] = (char)(((weightTot % 10000) / 1000) + 48);
        toWeightLCD[6] = (char)(((weightTot % 1000) / 100)+48);
        toWeightLCD[7] ='\0';        
       }
      if (CG != 0) {
        toCgLCD[0] = 'C';
        toCgLCD[1] = 'G';
        toCgLCD[2] = ':';
        toCgLCD[3] = (char)((CG / 10000)+48);
        toCgLCD[4] = (char)(((CG % 10000) / 1000)+48);
        toCgLCD[5] = (char)(((CG % 1000) / 100)+48);
        toCgLCD[6] = '.';
        toCgLCD[7] = (char)(((CG % 100) / 10)+48);
        toCgLCD[8] = '\0';
      }
      else {
        toCgLCD[0] = '\0';
      }

    /*--- Weight and Cg OLED display                ---*/
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,20);
    display.print(toWeightLCD);
    display.setCursor(0,40);
    display.print(toCgLCD);

    /*--- Battery value display                     ---*/
    int batval = readBattVoltage(&bBatWarning);
    snprintf(toBatLCD,11,"Bat:%d.%d%d V", (batval / 1000),((batval % 1000) / 100),((batval % 100) / 10));
    display.setTextSize(2);
    if(bBatWarning)
      ledState ? display.setTextColor(BLACK, WHITE) : display.setTextColor(WHITE);
    else
      display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.print(toBatLCD);
    display.display();

    #ifdef CG_SCALE_DEBUG
      /*--- DEBUG !!!                               ---*/  
      Serial.print(F("LCD Weight = "));
      Serial.print(toWeightLCD);
      Serial.print(F(" LCD CG = "));
      Serial.print(toCgLCD);
      Serial.print(F(" LCD Batt = "));
      Serial.println(toBatLCD);  
    #endif

    } /* end if output to OLED */

  } /* End if OLED/Serial interval */

  /*--- CG_SCALE stay alive !!!                     ---*/
  flashLED();

} /* end loop() */


