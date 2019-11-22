 /*
 * ESP8266 - Use the number that is in front of the GPIO 
 * or the constants A0, D0, D1, D2, D3, D4, D5, D6, D7, and D8.
 * ESP Pinout Diagram and full Arduino wiring diagram is in this folder
 * Instructions for ESP8266: https://www.instructables.com/id/noobs-guide-to-ESP8266-with-Arduino-Mega-2560-or-U/
 * Set up ESP8266 chip in Sketch Tab as:
 *  Tools -> Board -> NodeMCU 1.0 (ESP-12E Module)
 *  Tools -> Flash Size -> 4M (3M SPIFFS)
 *  Tools -> CPU Frequency -> 80 Mhz
 *  Tools -> Upload Speed -> 921600
 *  Tools-->Port--> COM13
*/

//ESP_8266 IP 199.5.20.155

#include <boarddefs.h>
#include <IRremote.h>
#include <IRremoteInt.h>
#include <ir_Lego_PF_BitStreamEncoder.h>
#include <stdio.h>

//General Constants
static const int TIMEOUT = 10000;
//TICK_Counter is calibrated to increase height variable once per sec
static const int TICK_COUNTER = 10000;
static const String CONFIG = "6789";//IR Receiver LEFT, UP, RIGHT, DOWN
int responseCheck = 0;
volatile bool STOPFLAG = true;

//Relay Module Pinout Values
static const int RELAY_MOT1_CW = 13;//Motor 1 Clockwise
static const int RELAY_MOT1_CCW = 12;//Motor 1 Counter Clockwise
static const int RELAY_MOT2_CW = 11;//Motor 2 Clockwise
static const int RELAY_MOT2_CCW = 10;//Motor 2 Counter Clockwise
static const int RELAY_MOT3_CW = 9;//Motor 3 Clockwise
static const int RELAY_MOT3_CCW = 8;//Motor 3 Counter Clockwise
static const int RELAY_7 = 7;//VACANT
static const int RELAY_8 = 6;//VACANT
static const int RELAY_MODULE_PWR = 5;

//Motor Control Variables
//All arrays are [Motor1, Motor2, Motor3]
//The motor is selected if the value is true
bool motorSelect[] = {false,false,false};

//The following array has an int object for each motor/screen
//each int object has 9 argument which represent their variables
//Program expects all screens to be at their lowest height.
//Motor variable sequence: 
//{CW Pin, CCW Pin, current screen height, max screen height (Programmable), configuration status}
static const int CW = 0;
static const int CCW = 1;
static const int CURRENT_HEIGHT = 2;
static const int TICKS = 3;
static const int MAX_HEIGHT = 4;
static const int CONFIG_STATUS = 5;
int motors[][6] = 
  {
  //   CW Pin             CCW Pin       Cur Ht    Ticks    Max Ht    Config Stat
  //      0                  1             2        3         4         5
    {RELAY_MOT1_CW,   RELAY_MOT1_CCW,      0,       0,       100,       0},//Left
    {RELAY_MOT2_CW,   RELAY_MOT2_CCW,      0,       0,       100,       0},//Center
    {RELAY_MOT3_CW,   RELAY_MOT3_CCW,      0,       0,       100,       0} //Right
  };

//IR Receiver Pinout Values
static const int IR_RCVR_G = A5;//GROUND PIN
static const int IR_RCVR_R = A4;//VCC PIN
static const int IR_RCVR_Y = A3;//SIGNAL PIN
IRrecv irrecv(IR_RCVR_Y);
decode_results results;

//ESP8266 Pinout Values
///Serial2 Pins on Board
static const int ESP_TX = 18;
static const int ESP_RX = 19;
static const int ESP_CLK = A15;
//static const int ESP_RST = XX; //Might need to declare this


void setup() {
  Serial.begin(9600);
  Serial2.begin(115200);

  //Setup IR Receiver pins
  pinMode(IR_RCVR_G, OUTPUT);
  pinMode(IR_RCVR_R, OUTPUT);
  pinMode(IR_RCVR_Y, INPUT);

  //Setup Relay Pins
  pinMode(RELAY_MOT1_CW, OUTPUT);
  pinMode(RELAY_MOT1_CCW, OUTPUT);
  pinMode(RELAY_MOT2_CW, OUTPUT);
  pinMode(RELAY_MOT2_CCW, OUTPUT);
  pinMode(RELAY_MOT3_CW, OUTPUT);
  pinMode(RELAY_MOT3_CCW, OUTPUT);
  pinMode(RELAY_MODULE_PWR, OUTPUT);

  //Setup ESP8266 Pins
  pinMode(ESP_TX, OUTPUT);
  pinMode(ESP_RX, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(ESP_RX), receiveData, CHANGE);

  
  //Constant pin writes
  digitalWrite(IR_RCVR_G, LOW);
  digitalWrite(IR_RCVR_R, HIGH);
  digitalWrite(RELAY_MODULE_PWR, HIGH);
  
  irrecv.enableIRIn(); // Start the receiver
}

//Need to add status to prevent running motors that haven't been configured
void loop() {  
  //IR Receiver Code Block  
  
  if (irrecv.decode(&results)) 
  {
    responseCheck = translateIR(); 
    //Serial.println(responseCheck);
    irrecv.resume();
  }
    
  switch(responseCheck)
  {
    case 2://SETTINGS prepares for configuration setup 
      Serial.println("SETTINGS button: Press LEFT UP RIGHT DOWN to begin programming.");
      irrecv.resume();
      programMotors();
      break;    
    case 6://Left Screen
      Serial.println("Left Screen Selected");
      irrecv.resume();
      motorSelect[0] = true;
      motorSelect[1] = false;
      motorSelect[2] = false;
      break;
    case 7://Front/Center Screen
      Serial.println("Front (Center) Screen Selected");
      irrecv.resume();
      motorSelect[0] = false;
      motorSelect[1] = true;
      motorSelect[2] = false;
      break;
    case 8://Right Screen
      Serial.println("Right Screen Selected");
      irrecv.resume();
      motorSelect[0] = false;
      motorSelect[1] = false;
      motorSelect[2] = true;
      break;   
    case 9://All Screens Selected
      Serial.println("All Screens Selected");
      irrecv.resume();
      motorSelect[0] = true;
      motorSelect[1] = true;
      motorSelect[2] = true;
      break;
    case 13://SOURCE prints variable values
      Serial.println("Variable Values:");
      irrecv.resume();
      reportVars();
      break;
    case 15://WOOFER_UP raises screens
      Serial.println("Raise Screen");
      irrecv.resume();
      raiseScreen();
      break;
    case 16://WOOFER_DOWN lowers screens
      Serial.println("Lower Screen");
      irrecv.resume();
      lowerScreen();
      break;
    case 17://WOOFER_MUTE stops screen motors
      Serial.println("All Screens Stop");
      irrecv.resume();
      stopScreens();
      break;
  }
  responseCheck = 0;
  tick();

    readDigitalPins();
}//end loop

//Reads digital pin values
void readDigitalPins()
{
  for (int x = 0; x < 3; x++)
    {
      //Emergency Value Check
      if(digitalRead(motors[x][CW]) == HIGH && digitalRead(motors[x][CCW]) == HIGH)
      {
        char emergencyOutput[100];
        sprintf(emergencyOutput, "Fatal Error. Both the CW and CCW Pin for Motor %d were HIGH which can severely damage the motor. Hardware Reset is Required.", x);
        Serial.println(emergencyOutput);
        digitalWrite(motors[x][CW], LOW);
        digitalWrite(motors[x][CCW], LOW);
        stopScreens();

        for (int x = 0; x < 3; x++)
        {
          motors[x][CONFIG_STATUS] = 0;
          motors[x][CURRENT_HEIGHT] = 0;
        }
        
        do
        {
          //Endless loop here, requires hardware interrupt to break.
        }
        while(STOPFLAG);

        resetFlagIRQ();
      }
      
//   CW Pin             CCW Pin       Cur Ht   Max Ht    Config Stat
//      0                  1             2        3        4     
      //Motor is too high, retracts to max height
      if(motors[x][CURRENT_HEIGHT] > motors[x][MAX_HEIGHT])
      {
        stopScreens();
        digitalWrite(motors[x][CCW], HIGH);
        char highMotor[100];
        sprintf(highMotor, "Motor %d is too high. Current Height: %d. Max Height: %d. Lowering to safe height.", (x+1), motors[x][CURRENT_HEIGHT], motors[x][MAX_HEIGHT]);
        Serial.println(highMotor);
        do
        {
          tick();
        }
        while(motors[x][CURRENT_HEIGHT] > motors[x][MAX_HEIGHT]);
        sprintf(highMotor, "Motor %d retracted to height %d. To modify the max height, enter configuration mode.", (x+1), motors[x][CURRENT_HEIGHT]);
        Serial.println(highMotor);
        digitalWrite(motors[x][CCW], LOW);
      }

      //Motor is too low, retracts to min height
      if(motors[x][CURRENT_HEIGHT] < 0)
      {
        stopScreens();
        digitalWrite(motors[x][CW], HIGH);
        char lowMotor[100];
        sprintf(lowMotor, "Motor %d is too low. Current Height: %d. Max Height: %d. Raising to safe height.", (x+1), motors[x][CURRENT_HEIGHT], motors[x][MAX_HEIGHT]);
        Serial.println(lowMotor);
        do
        {
          tick();
        }
        while(motors[x][CURRENT_HEIGHT] < 0);
        digitalWrite(motors[x][CW], LOW);
        sprintf(lowMotor, "Motor %d has been raised to height %d.", (x+1), motors[x][CURRENT_HEIGHT]);
        Serial.println(lowMotor);
      }

      tick();
   }
}

//Counter Clockwise Rotation lowers the screen
void lowerScreen()
{
  for (int x = 0; x < 3; x = x + 1)
  {
    if(motorSelect[x] && motors[x][CONFIG_STATUS] == 1)
    {
      digitalWrite(motors[x][CW], LOW);
      digitalWrite(motors[x][CCW], HIGH);
    }
  }
}

//Clockwise Rotation raises the screen
void raiseScreen()
{
  for (int x = 0; x < 3; x = x + 1)
  {
    if(motorSelect[x] && motors[x][CONFIG_STATUS] == 1)
    {
      digitalWrite(motors[x][CCW], LOW);
      digitalWrite(motors[x][CW], HIGH);
    }
  }
}

void stopScreens()
{
  for (int x = 0; x < 3; x = x + 1)
  {
    digitalWrite(motors[x][CW], LOW);
    digitalWrite(motors[x][CCW], LOW);
  }
}

//motor timer tick
void tick()
{
  for (int x = 0; x < 3; x++)
  {
    if(digitalRead(motors[x][CW]) == HIGH)
    {
      motors[x][TICKS]++;
    }
    if(digitalRead(motors[x][CCW]) == HIGH)
    {
      motors[x][TICKS]--;
    }
    if(motors[x][TICKS] >= TICK_COUNTER)
    {
      motors[x][TICKS] = 0;
      motors[x][CURRENT_HEIGHT]++;
    }
    if(motors[x][TICKS] < 0)
    {
      motors[x][TICKS] = (TICK_COUNTER - 1);
      motors[x][CURRENT_HEIGHT]--;
    }
  }
}

/*TODO: Reports Required:
 * WiFi Connection Status
 */
void reportVars()
{
  char motorsReport[240];
  char wifiStatus[240];

  for (int x = 0; x < 3; x = x + 1)
  {
    char* cwVal;
    char* ccwVal;
    char* configStatus;

    //Clockwise Pin
    if (digitalRead(motors[x][CW]) == LOW)
    {
      cwVal = "Off";
    }
    else
    {
      cwVal = "On";
    }

    //CounterClockwise Pin
    if (digitalRead(motors[x][CCW]) == LOW)
    {
      ccwVal = "Off";
    }
    else
    {
      ccwVal = "On";
    }

    //Configuration Status
    if (motors[x][CONFIG_STATUS] == 0)
    {
      configStatus = "Not Configured";
    }
    else
    {
      configStatus = "Configured";
    }
    
    sprintf(motorsReport, "Motor %d - CW Pin: %s, CCW Pin: %s, Configuration Status: %s, Current Height: %d, Max Height: %d",
      (x+1),
      cwVal, 
      ccwVal, 
      configStatus,
      motors[x][CURRENT_HEIGHT],
      motors[x][MAX_HEIGHT]
    );
    Serial.println(motorsReport);
    Serial.println();

    Serial2.write(motorsReport);
  }
}

//Interrupt function to receive data from ESP8266 
//Converts received data into command
void receiveData()
{
 int command = Serial2.read(); 
 switch(command)
  {
    case 0://Request Motor Status 
      Serial2.write("Received 0: Motor Status");
      irrecv.resume();
      reportVars();
      break;    
    case 1://Left Screen
      Serial2.write("Received 1: Left Screen Selected");
      irrecv.resume();
      motorSelect[0] = true;
      motorSelect[1] = false;
      motorSelect[2] = false;
      break;
    case 2://Front/Center Screen
      Serial2.write("Received 2: Front (Center) Screen Selected");
      irrecv.resume();
      motorSelect[0] = false;
      motorSelect[1] = true;
      motorSelect[2] = false;
      break;
    case 3://Right Screen
      Serial2.write("Received 3: Right Screen Selected");
      irrecv.resume();
      motorSelect[0] = false;
      motorSelect[1] = false;
      motorSelect[2] = true;
      break;   
    case 4://All Screens Selected
      Serial2.write("Received 4: All Screens Selected");
      irrecv.resume();
      motorSelect[0] = true;
      motorSelect[1] = true;
      motorSelect[2] = true;
      break;
    case 5://WOOFER_UP raises screens
      Serial2.write("Received 5: Raise Screen");
      irrecv.resume();
      raiseScreen();
      break;
    case 6://WOOFER_DOWN lowers screens
      Serial2.write("Received 6: Lower Screen");
      irrecv.resume();
      lowerScreen();
      break;
    case 7://WOOFER_MUTE stops screen motors
      Serial2.write("Received 7: All Screens Stop");
      irrecv.resume();
      stopScreens();
      break;
    case 8://Reset from Critical Malfunction
      Serial2.write("Received 8: Reset after Critical Malfunction");
      irrecv.resume();
      stopFlagIRQ();
      break;
  }
}

//Motor Programming Function
void programMotors()
{
  Serial.println("Program Motors Function Initiated. To continue, press the correct button sequence on the remote.");
  bool doLoopCheck = true;
  String thisResponse;
  int timer = millis();
  int inputCount = 0;
  irrecv.resume();
  
  do{
    //waits for response until appropriate action is received
    //System exits configuration mode if computer doesn't receive input quick enough
    int timerComp = millis();
    if (timerComp - timer >= TIMEOUT)
    {
      Serial.println("Timed out. Exiting System Setup.");
      doLoopCheck = false;
      break;
    }

    //Waits for input, concats to end of string
    if (irrecv.decode(&results)) 
    {
      thisResponse += translateIR(); 
      Serial.println(thisResponse);
      timer = millis();
      inputCount++;
      irrecv.resume();
    } 

    //If four inputs have been received, the loop ends
    if(inputCount == 4)
    {
      doLoopCheck = false;
      irrecv.resume();
      break;
    }
  }
  while(doLoopCheck);

  //begins configuration mode if inputs match system configuration combo
  if(thisResponse.compareTo(CONFIG) == 0)
  {
    Serial.println("Configuration Mode Open. Please select a motor.");
    Serial.println("LEFT is Motor 1, UP is Motor 2, RIGHT is Motor 3. Press SETTINGS to exit Configuration Mode.");
    doLoopCheck = true;
    timer = millis();
    int intResponse = 0;
    bool hasResponse = false;
    
    do
    {
      int timerComp = millis();
      if (timerComp - timer >= TIMEOUT)
      {
        Serial.println("Timed out. Exiting System Setup.");
        doLoopCheck = false;
        break;
      }
      
      if (irrecv.decode(&results)) 
      {
        intResponse = translateIR(); 
        irrecv.resume();
        timer = millis();
        hasResponse = true;
      } 
      
      if (hasResponse)      
      {
        switch(intResponse)
        {
          case 2:
            Serial.println("Exiting Configuration Mode.");
            doLoopCheck = false;
            break;
          case 6: 
            Serial.println("Left Screen Selected");
            motorSelect[0] = true;
            motorSelect[1] = false;
            motorSelect[2] = false;
            doLoopCheck = false;
            configScreens(0);
            break;  
          case 7: 
            Serial.println("Center Screen Selected");
            motorSelect[0] = false;
            motorSelect[1] = true;
            motorSelect[2] = false;
            doLoopCheck = false;
            configScreens(1);
            break; 
          case 8: 
            Serial.println("Right Screen Selected");
            motorSelect[0] = false;
            motorSelect[1] = false;
            motorSelect[2] = true;
            doLoopCheck = false;
            configScreens(2);
            break; 
          
          default: 
            Serial.println("INVALID INPUT");
            break;
        }
        hasResponse = false;
      }
    }
    while(doLoopCheck);
  }
  
  else
  {
    Serial.println("NO INPUT RECEIVED. Exiting Configuration Mode.");
  }
}

//Configuration Mode
void configScreens(int motVal)
{
  bool openConfig = true;
  Serial.println("Raise Screen by pressing WOOFERUP. Lower Screen by pressing WOOFERDOWN. Stop the Screen by pressing WOOFERMUTE.");
  Serial.println("When the screen is at the highest point, press SETTINGS.");

  motors[motVal][CONFIG_STATUS] = 1;

  //track whether cw and ccw are high/low here
  do
  {
      int intResponse = 0;      
      if (irrecv.decode(&results)) 
      {
          intResponse = translateIR(); 
          irrecv.resume();
      }

      switch(intResponse)
      {
        case 2://SETTINGS prepares for configuration setup 
          Serial.println("Configuration Complete. Exiting Configuration Mode.");
          motors[motVal][MAX_HEIGHT] = motors[motVal][CURRENT_HEIGHT];
          openConfig = false;
          break;
        case 13://SOURCE prints variable values
          Serial.println("Variable Values:");
          reportVars();
          break;
        case 15://WOOFER_UP raises screens
          Serial.println("Raise Screen");
          digitalWrite(motors[motVal][CCW], LOW);
          digitalWrite(motors[motVal][CW], HIGH);
          break;
        case 16://WOOFER_DOWN lowers screens
          Serial.println("Lower Screen");
          digitalWrite(motors[motVal][CW], LOW);
          digitalWrite(motors[motVal][CCW], HIGH);
          break;
        case 17://WOOFER_MUTE stops screen motors
          Serial.println("Motor Stop");
          digitalWrite(motors[motVal][CW], LOW);
          digitalWrite(motors[motVal][CCW], LOW);
          break;
        default:
          break;
      }

      tick();
  }while(openConfig);
}//End configScreens();

int translateIR() // takes action based on IR code received
{
  //Serial.println(results.value, HEX);
  int response = 0;
  
  switch(results.value)
    {
      case 0xCA31DA45: response = 1;  break;//Serial.println("POWER");
      case 0xBF1BC226: response = 2;  break;//Begins configuration mode Serial.println("SETTINGS");
      case 0xCF2F9DAB: response = 3;  break;//Serial.println("VOL+");
      case 0x28DE45AA: response = 4;  break;//Serial.println("PLAY/PAUSE");
      case 0xB2BBAC69: response = 5;  break;//Serial.println("VOL-");
      case 0x879b92C2: response = 6;  break;//Left Screen Selected Serial.println("LEFT");
      case 0x65DC8646: response = 7;  break;//Front/Center Screen Selected Serial.println("UP");
      case 0x46868606: response = 8;  break;//Right Screen Selected Serial.println("RIGHT");
      case 0xCC112BC2: response = 9;  break;//ALL Screens selected Serial.println("DOWN");
      case 0xD65A38A: response = 10;  break;//Serial.println("BLUETOOTH_POWER");
      case 0xC0CDDA6: response = 11;  break;//Serial.println("SURROUND");
      case 0xF9F925C3: response = 12;  break;//Serial.println("SOUND_MODE");
      case 0x17112D07: response = 13;  break;//Print Variable Values Serial.println("SOURCE");
      case 0x123CD34B: response = 14; break;//Serial.println("MUTE"); 
      case 0x6F15FF46: response = 15;  break;//raises selected motor Serial.println("WOOFER_UP");
      case 0xFB758842: response = 16;  break;//lowers selected motor Serial.println("WOOFER_DOWN");
      case 0x7C101285: response = 17;  break;//stops selected motor Serial.println("WOOFER_MUTE");
      case 0xFFFFFFFF: Serial.println(" REPEAT"); break;//

      default: 
        Serial.println("IR Code was not recognized. "); break;
  }// End Case
  
  return response;
  delay(1200); // Do not get immediate repeat
} //END translateIR

//Following should only be called if emergency shutdown was required
void stopFlagIRQ()
{
  STOPFLAG = false;
  Serial.println("Stop Flag Interrupt Received, Loop Broken.");
}

void resetFlagIRQ()
{
  STOPFLAG = true;
}
