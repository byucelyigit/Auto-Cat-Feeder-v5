#include <Arduino.h>
#include <U8g2lib.h>
#include <CheapStepper.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h> //oled
#include "HX711.h" //scale

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
const int button1Pin = 3; 
const int button2Pin = 2;               
const int button3Pin = 4; //mode
const int stopPin = 5;
const int slideDistance = 1750;

int buttonStatus = 0;
int doorStatus = 0;
bool positionKnown = false;

int mode = 0;
int timerCount = 0;
bool button3Pressed = false;
bool button2Pressed = false;
bool button1Pressed = false;
int alarmHr = 0;
int alarmMin = 0;
int clockHr = 0;
int clockMin = 0;

#define ModeDoNothing 0
#define ModeDisplayInit 1
#define ModeInitPos 2
#define ModeDisplayOpening 3
#define ModeRunForOpen 4
#define ModeDisplayClosing 5
#define ModeRunForClose 6
#define ModeInitPosAchieved 7
#define ModeTimeForFood 8
#define ModeEndOfTimeForFood 9
#define ModeError 10

#define DoorStatusUnkonwn 0
#define DoorStatusClosed 1
#define DoorStatusopen 2

#define ButtonStatusOpenClose 0
#define ButtonStatusSetTime 1
#define ButtonStatusSetAlarm 2



//used for oled display
void DrawToOled(int x, int y, const char *s)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(x,y,s);    
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
    DrawToOled(2, 30, datestring);
}

void printTime(int x, int y, const RtcDateTime& dt)
{
    char datestring[10];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u:%02u:%02u"),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
    DrawToOled(x, y, datestring);
}




void setup() {


  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  //oled related
  u8g2.begin();


  //scale related 
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  
  //Clock related
  #pragma region ClockRelated  

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

}


void loop() {

  //Clock *********************************************
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

  RtcDateTime now = Rtc.GetDateTime();

  Serial.println();
  clockMin = now.Minute();
  clockHr = now.Hour();



//Scale **************************************************
  if (scale.is_ready()) {
    long reading = scale.read();
    Serial.print("HX711 reading: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711 not found.");
  }


// Buttons and states
#pragma region buttonsStates
  int buttonState1 = digitalRead(button1Pin);
  int buttonState2 = digitalRead(button2Pin);
  int buttonState3 = digitalRead(button3Pin);
  int stopPinState = digitalRead(stopPin);
  if(mode==ModeDoNothing || mode == ModeError)
  {
    if((clockMin==alarmMin) && (clockHr==alarmHr) && (now.Second() == 0))
    {
      mode = ModeTimeForFood;
      Serial.println("ALARM");
    }
  }

  if(buttonState3==1)
  {
    button3Pressed = false;
  }

  if (buttonState3 == 0) {
    //mode = 1; // motor runs
    if(!button3Pressed)
    {
      switch(buttonStatus) {
        case ButtonStatusSetTime: 
          buttonStatus = ButtonStatusSetAlarm;
          RtcDateTime newTime = RtcDateTime(2019,1,1,clockHr,clockMin,0);
          Rtc.SetDateTime(newTime);
          break;
        case ButtonStatusSetAlarm:
          buttonStatus = ButtonStatusOpenClose;
          //burada alarm değeri değişmiş ise eproma kayıt etmesi lazım. elektrik gidince yeniden okur
          break;
        case ButtonStatusOpenClose:
          buttonStatus = ButtonStatusSetTime;
          break;
      }
      button3Pressed = true;
    }
  }

  if(buttonState2 == 1)
  { 
    button2Pressed = false;
  }

  if (buttonState2 == 0) {
    if(!button2Pressed)
    {
      if(buttonStatus==ButtonStatusSetAlarm)
      {
        alarmMin = alarmMin + 10;
        if(alarmMin==60) {alarmMin = 0;}
      }
      if(buttonStatus == ButtonStatusSetTime)
      {
        clockMin = now.Minute();
        clockMin = clockMin + 1;
        if(clockMin == 60) {clockMin = 0;}
        RtcDateTime newTime = (2019,1,21,now.Hour(),clockMin,0);
        Rtc.SetDateTime(newTime);
      }
    }
    button2Pressed = true;
  }

  if(buttonState1 == 1)
  { 
    button1Pressed = false;
  }

  if (buttonState1 == 0) {
    timerCount = 0;
    if(!button1Pressed)
    {
      if(buttonStatus==ButtonStatusSetTime)
      {
        clockHr = now.Hour();
        clockHr = clockHr + 1;
        if(clockHr == 24) { clockHr = 0;}
        RtcDateTime newTime = (2019,1,21,clockHr,now.Minute(),0);
        Rtc.SetDateTime(newTime);        
      }

      if(buttonStatus==ButtonStatusSetAlarm)
      {
        alarmHr = alarmHr + 1;
        if(alarmHr == 24) { alarmHr = 0;}
      }

      if(buttonStatus==ButtonStatusOpenClose)
      {
        if(mode==ModeDoNothing || mode == ModeError)
        {
          if(positionKnown)
          {
            if(doorStatus == DoorStatusClosed) 
            {
              mode = ModeDisplayOpening;
            }
            else //DoorOpen
            {
              mode = ModeDisplayClosing;
            }
          }
          else
          {
            mode = ModeDisplayInit;      
          }
        }
      }
    }
    button1Pressed = true;
  }

  //check if door reached init pos. 
  //This is needed for first time zero point detection
  if (stopPinState == 0) {
    if(mode==ModeInitPos)
    {
      mode = ModeInitPosAchieved;
    }
  }

#pragma endregion buttonStates

// Modes 
#pragma region Modes

  if(mode==ModeInitPosAchieved)
  {
    positionKnown = true; //emergency stop
    doorStatus = DoorStatusClosed;
    mode = ModeDisplayOpening; //door is opening
    timerCount = 0;
  }

  if(mode==ModeTimeForFood) //yemek vakti
  {
    mode = ModeDisplayOpening;
    delay(2000);  //tekrar alarm tetiklenmesin diye 2 saniye sonra açar  
    buttonStatus = ButtonStatusOpenClose;    
  }

  if(mode == ModeInitPos)
  {
    stepper.newMoveTo(moveClockwise, 50);
    timerCount = timerCount +1;
  }

  if(timerCount > 50)
  {
    mode = ModeError;
  }

  if(mode == ModeDisplayInit)
  {
    mode = ModeInitPos;
  }

  if(mode == ModeRunForOpen)
  {
    stepper.newMoveTo(moveClockwise, -slideDistance);
    mode = ModeDoNothing;
    doorStatus = DoorStatusopen;
  }

  if(mode == ModeDisplayOpening)
  {
    if(doorStatus == DoorStatusClosed)
    {
      mode = ModeRunForOpen;
      doorStatus = DoorStatusUnkonwn;
    }
  }

  if(mode == ModeRunForClose)
  {
    stepper.newMoveTo(moveClockwise, slideDistance);
    mode = ModeDoNothing;
    doorStatus = DoorStatusClosed;
  }

  if(mode == ModeDisplayClosing)
  {
    mode = ModeRunForClose;
    doorStatus = DoorStatusUnkonwn;
  }

#pragma endregion Modes

//display statüs ve other information

      u8g2.setFont(u8g2_font_9x15B_mf);
      switch(mode) {
        case ModeDoNothing: DrawToOled(0,0,"...");break;
        case ModeDisplayInit: DrawToOled(0,0,"Initializing...");break;
        case ModeInitPos: DrawToOled(0,0,"Initializing...");break;        
        case ModeDisplayOpening: DrawToOled(0,0,"Opening...");break;        
        case ModeRunForOpen: DrawToOled(0,0,"Open");break;        
        case ModeDisplayClosing: DrawToOled(0,0,"Closing...");break;                
        case ModeRunForClose: DrawToOled(0,0,"Closed");break;                        
        case ModeInitPosAchieved: DrawToOled(0,0,"Ready");break; 
        case ModeTimeForFood: DrawToOled(0,0,"Time for food");break;                                                              
        case ModeEndOfTimeForFood: DrawToOled(0,0,"Time for food");break;
        case ModeError: DrawToOled(0,0,"Error");break;
      }

      printTime(2, 30, now);
      printTime(3, 30, RtcDateTime(2000,1, 1, alarmHr, alarmMin, 0));




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







