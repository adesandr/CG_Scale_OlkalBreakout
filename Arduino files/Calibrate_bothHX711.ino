//-------------------------------------------------------------------------------------
// HX711_ADC.h
// Arduino master library for HX711 24-Bit Analog-to-Digital Converter for Weigh Scales
// Olav Kallhovd sept2017
// Tested with      : HX711 asian module on channel A and YZC-133 3kg load cell
// Tested with MCU  : Arduino Nano
//-------------------------------------------------------------------------------------
/* This is an example sketch on how to find correct calibration factor for your HX711:
   - Power up the scale and open Arduino serial terminal
   - After stabelizing and tare is complete, put a known weight on the load cell
   - Observe values on serial terminal
   - Adjust the calibration factor until output value is same as your known weight:
      - Sending 'l' from the serial terminal decrease factor by 1.0
      - Sending 'L' from the serial terminal decrease factor by 10.0
      - Sending 'h' from the serial terminal increase factor by 1.0
      - Sending 'H' from the serial terminal increase factor by 10.0
      - Sending 't' from the serial terminal call tare function
   - Observe and note the value of the new calibration factor
   - Use this new calibration factor in your sketch
*/

#include <HX711_ADC.h>

//HX711 constructor (dout pin, sck pin)
//HX711 constructor (dout pin, sck pint):
HX711_ADC LoadCell_1(A2, A3); //HX711 pins front sensor (DOUT, PD_SCK)
HX711_ADC LoadCell_2(A0, A1); //HX711 pins rear sensor (DOUT, PD_SCK)

#define NB_LOADCELL 2    /*--- Load Cell number    ---*/

long t;                   /*--- Loop period control ---*/

int num_LoadCell = 0;     /*--- Load cell number. Front cell by default to start ---*/

int i;                    /*--- Loop control ---*/

float LoadCellValue[NB_LOADCELL];
float LoadCellFactor[] = {696.0,696.0};

const long stabilisingtime = 3000; // tare precision can be improved by adding a few seconds of stabilising time

void setup() {
  Serial.begin(9600);
  Serial.println("Wait...");
  LoadCell_1.begin();
  LoadCell_2.begin();
  
   /*--- HX711 init. and calibration factor setting ---*/
  LoadCell_1.begin();
  LoadCell_2.begin();
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  while ((loadcell_1_rdy + loadcell_2_rdy) < 2) { //run startup, stabilisation and tare, both modules simultaneously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilisingtime);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilisingtime);
  }
  
  LoadCell_1.setCalFactor(LoadCellFactor[0]); // set calibration factor
  LoadCell_2.setCalFactor(LoadCellFactor[1]); // set calibration factor
  
  Serial.println("Startup + tare is complete ...");
}

void loop() {
  //update() should be called at least as often as HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS
  //longer delay in scetch will reduce effective sample rate (be carefull with delay() in loop)
  LoadCell_1.update();
  LoadCell_2.update();

  //get smoothed value from data set + current calibration factor
  if (millis() > t + 2000) {

 
    
    for(i = 0; i < NB_LOADCELL; i++)
    {
      if(i == 0)
      {
        LoadCellValue[i] = LoadCell_1.getData();
        LoadCellFactor[i] = LoadCell_1.getCalFactor();
      }
      else
      {
        LoadCellValue[i] = LoadCell_2.getData();
        LoadCellFactor[i] = LoadCell_2.getCalFactor();
      }

    Serial.print("Load_cell output val ");
    i==0 ? Serial.print("FRONT:") : Serial.print("REAR:");
    Serial.print(LoadCellValue[i]);
    Serial.print("      Load_cell calFactor: ");
    i==0 ? Serial.print("FRONT:") : Serial.print("REAR:");
    Serial.println(LoadCellFactor[i]);

    }
    t = millis();
  }

  //receive from serial terminal
  if (Serial.available() > 0) {
 
    float j = 0;
    char inByte = Serial.read();
    if (inByte == 'F') num_LoadCell = 0;
    else if (inByte == 'R') num_LoadCell = 1;
    else if (inByte == 'l') j = -1.0;
    else if (inByte == 'L') j = -10.0;
    else if (inByte == 'h') j = 1.0;
    else if (inByte == 'H') j = 10.0;
    else if (inByte == 't') 
    {
      if (num_LoadCell == 0)
        LoadCell_1.tareNoDelay();
      else
        LoadCell_2.tareNoDelay();
    }
    if (j != 't') {
      if(num_LoadCell == 0)
      {
      LoadCellFactor[num_LoadCell] = LoadCell_1.getCalFactor() + j;
      LoadCell_1.setCalFactor(LoadCellFactor[num_LoadCell]);
      }
      else
      {
      LoadCellFactor[num_LoadCell] = LoadCell_2.getCalFactor() + j;
      LoadCell_2.setCalFactor(LoadCellFactor[num_LoadCell]);
      }
 
    }

        // Welcome Banner
    Serial.print("---------- CALIBRATION ");
    num_LoadCell == 0 ? Serial.println("FRONT ----------") : Serial.println("REAR ----------");

  }
   
  //check if last tare operation is complete
  if(num_LoadCell == 0)
  {
     if (LoadCell_1.getTareStatus() == true) {
        Serial.println("Tare complete FRONT");
    }
  }
  else
  {
     if (LoadCell_2.getTareStatus() == true) {
        Serial.println("Tare complete REAR");
    }
  }

}
