#include <Arduino.h>
#include <U8g2lib.h>
#include <CheapStepper.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_64X32_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_64X32_1F_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SH1106_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
//U8G2_SSD1306_128X64_NONAME_1_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);


#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
#include "HX711.h"

RtcDS1307<TwoWire> Rtc(Wire);

CheapStepper stepper (8,9,10,11);  

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;

HX711 scale;

 // let's also create a boolean variable to save the direction of our rotation
 // and a timer variable to keep track of move times
bool moveClockwise = true;
unsigned long moveStartTime = 0; // this will save the time (millis()) when we started each new move

void draw(const char *s)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(2,30,s);    
    //u8g2.drawFrame(0,0,u8g2.getDisplayWidth(),u8g2.getDisplayHeight() );
  } while ( u8g2.nextPage() );
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
    draw(datestring);
}

void printTime(const RtcDateTime& dt)
{
    char datestring[10];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u:%02u:%02u"),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
    draw(datestring);
}


void setup() {

  #pragma region ClockRelated  

  pinMode(LED_BUILTIN, OUTPUT);
  u8g2.begin();
  u8g2.setFont(u8g2_font_9x15B_mf);

  Serial.begin(9600);

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();


  //clock related code from lib sample 
  if (!Rtc.IsDateTimeValid()) 
  {
      if (Rtc.LastError() != 0)
      {
          // we have a communications error
          // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
          // what the number means
          Serial.print("RTC communications error = ");
          Serial.println(Rtc.LastError());
      }
      else
      {
          // Common Causes:
          //    1) first time you ran and the device wasn't running yet
          //    2) the battery on the device is low or even missing

          Serial.println("RTC lost confidence in the DateTime!");
          // following line sets the RTC to the date & time this sketch was compiled
          // it will also reset the valid flag internally unless the Rtc device is
          // having an issue

          Rtc.SetDateTime(compiled);
      }
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
 
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 


 #pragma endregion ClockRelated

  #pragma region StepperRelated

  //*********************************************************************
  //stepper ralated code from lib sample
  Serial.print("stepper RPM: "); Serial.print(stepper.getRpm());
  Serial.println();

  // and let's print the delay time (in microseconds) between each step
  // the delay is based on the RPM setting:
  // it's how long the stepper will wait before each step

  Serial.print("stepper delay (micros): "); Serial.print(stepper.getDelay());
  Serial.println(); Serial.println();

  // now let's set up our first move...
  // let's move a half rotation from the start point

  stepper.newMoveTo(moveClockwise, 2048);
  /* this is the same as: 
    * stepper.newMoveToDegree(clockwise, 180);
    * because there are 4096 (default) steps in a full rotation
    */
  moveStartTime = millis(); // let's save the time at which we started this move

  #pragma endregion StepperRelated

  #pragma region scale
  
  //scale related 
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  #pragma endregion scale

}



void loop() {
  
  /*
  u8g2.setPowerSave(0);
  draw("power on / x");
  u8g2.setPowerSave(1);
  draw("power off / x");
  u8g2.setPowerSave(0);
  draw("power on / +");
  u8g2.setPowerSave(1);
  draw("power off / +");
  */


    if (!Rtc.IsDateTimeValid()) 
    {
        if (Rtc.LastError() != 0)
        {
            // we have a communications error
            // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
            // what the number means
            Serial.print("RTC communications error = ");
            Serial.println(Rtc.LastError());
        }
        else
        {
            // Common Causes:
            //    1) the battery on the device is low or even missing and the power line was disconnected
            Serial.println("RTC lost confidence in the DateTime!");
        }
    }

  if (scale.is_ready()) {
    long reading = scale.read();
    Serial.print("HX711 reading: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711 not found.");
  }


    RtcDateTime now = Rtc.GetDateTime();

    printTime(now);
    Serial.println();
    //delay(1000); // ten seconds

  // we need to call run() during loop() 
  // in order to keep the stepper moving
  // if we are using non-blocking moves
  
  stepper.run();

  ////////////////////////////////
  // now the stepper is moving, //
  // let's do some other stuff! //
  ////////////////////////////////

  // let's check how many steps are left in the current move:
  
  int stepsLeft = stepper.getStepsLeft();

  // if the current move is done...
  
  if (stepsLeft == 0){

    // let's print the position of the stepper to serial
    
    Serial.print("stepper position: "); Serial.print(stepper.getStep());
    Serial.println();

    // and now let's print the time the move took

    unsigned long timeTook = millis() - moveStartTime; // calculate time elapsed since move start
    Serial.print("move took (ms): "); Serial.print(timeTook);
    Serial.println(); Serial.println();
    
    // let's start a new move in the reverse direction
    
    moveClockwise = !moveClockwise; // reverse direction
    stepper.newMoveDegrees (moveClockwise, 180); // move 180 degrees from current position
    moveStartTime = millis(); // reset move start time

  }



}





