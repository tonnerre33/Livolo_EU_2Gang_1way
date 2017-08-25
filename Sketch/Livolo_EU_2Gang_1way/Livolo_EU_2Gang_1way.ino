//Fuse L:E2 H:DA E:05 
//// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69


//#define MY_DEBUG
//#define MY_OTA_FIRMWARE_FEATURE  // Enables OTA firmware updates if DualOptiBoot

#define MY_NODE_ID 2
//#define MY_DISABLED_SERIAL
//#define MY_TRANSPORT_WAIT_READY_MS 1000
/*#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC*/
#include <MySensors.h>

#define NUMBER_OF_BUTTONS 2 // Total number of attached relays


int ledPins[] = {6,A1}; //{LED BP LEFT, LED BP RIGHT}
byte buttonPins[] = {3,A0}; //{BP LEFT, BP RIGHT}
const uint8_t RELAY_CH_PINS[][2] = {
    {5, 4}, // channel 1 relay control pins(bistable relay - 2 coils) {L1_SET, L1_RST}
    {A3, A4}  // channel 2 relay control pins(bistable relay - 2 coils) {L2_SET, L2_RST}
};
#define MTSA_PIN A5
#define MTPM_PIN 7
#define BUZZER_PIN A2


const uint32_t RELAY_PULSE_DELAY_MS = 40; //Def 50

#define OFF 0
#define ON  1
#define SET_COIL_INDEX     0
#define RESET_COIL_INDEX   1
#define RELEASED  0
#define TOUCHED   1
const uint32_t SHORT_TOUCH_DETECT_THRESHOLD_MS = 50; 
const uint32_t LONG_TOUCH_DETECT_THRESHOLD_MS = 2000;
const uint32_t MODE_TIMER_MS = 60000;

bool buttonStates[] = {false, false};
long buttonLastChange[] = {OFF, OFF};
uint8_t channelState[] = {OFF, OFF};
bool changedStates[] = {false, false};
bool trigger = false;
uint32_t lastSwitchLight = -1;
uint32_t lastReceivCom = -1;
uint32_t lastOnCde[NUMBER_OF_BUTTONS];

#define MODE_NORMAL 0
#define MODE_TIMER 1
uint8_t mode[] = {MODE_NORMAL, MODE_NORMAL};


MyMessage msg(1, V_TRIPPED);
MyMessage msgdebug(1, V_TEXT);


void before()
{

   
  // initialize led, relays pins as outputs and buttons as inputs
  for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    //SENSIBILITY
    pinMode(MTSA_PIN, OUTPUT);
    digitalWrite(MTSA_PIN, HIGH);
    //POWER MODE
    pinMode(MTPM_PIN, OUTPUT);
    digitalWrite(MTPM_PIN, HIGH);
    //BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    pinMode(ledPins[i], OUTPUT);
    blinkLedFastly(3, ledPins[i]);
    digitalWrite(ledPins[i], buttonStates[i]);
    pinMode(buttonPins[i], INPUT);
    digitalWrite(buttonPins[i], HIGH);       // turn on pullup resistors
    
   // pinMode(relayPins[i], OUTPUT);
    for (int j = 0; j < NUMBER_OF_BUTTONS; j++) {
      pinMode(RELAY_CH_PINS[i][j], OUTPUT);
       
    }
  }



  // initialize extra leds as outputs
  //pinMode(EXTRA_LED1_PIN, OUTPUT);
 // pinMode(EXTRA_LED2_PIN, OUTPUT);
}

void presentation()
{
  // Send the sketch version information to the gateway and Controller

 
  sendSketchInfo("LivoloRelay2", "1.83");

  // Register sensors to gateway
  for (int j = 0; j < NUMBER_OF_BUTTONS; j++) {
    present(j+1, S_BINARY);
  }


 present(3, S_INFO); 
 // present(EXTRA_LED1_ID, S_BINARY);
 // present(EXTRA_LED2_ID, S_BINARY);

//sleep(30000);
//  for (int l = 1; l <= NUMBER_OF_BUTTONS; l++) {
//    switchLight(l, loadState(l));
//  }
//  switchLight(EXTRA_LED1_ID, loadState(EXTRA_LED1_ID));
//  switchLight(EXTRA_LED2_ID, loadState(EXTRA_LED2_ID));
}

void receive(const MyMessage &message)
{


    lastReceivCom = millis();
    // We only expect one type of message from controller. But we better check anyway.
   if (message.type==V_STATUS) {
      if (message.sensor<=NUMBER_OF_BUTTONS) {
        mode[message.sensor-1] = MODE_NORMAL;
        switchLight(message.sensor, message.getBool());
      }
  
  #ifdef MY_DEBUG
      // Write some debug info
      Serial.print("Incoming change for sensor:");
      Serial.print(message.sensor);
      Serial.print(", New status: ");
      Serial.println(message.getBool());
  #endif
    }
  
}

/// Change status of "switch" or extra led
void switchLight(int sensorID, bool newStatus) {
    // if this is a physical relay, we need to switch on the relay
    if (sensorID<=NUMBER_OF_BUTTONS) {
        setChannelRelaySwitchState(sensorID-1, newStatus);
        lastSwitchLight = millis();
        if(newStatus == ON) { lastOnCde[sensorID-1] = millis();  buttonStates[sensorID-1] = true;}
        else { buttonStates[sensorID-1] = false; }
        changedStates[sensorID-1]  = true;    
    }

    
    /*if (sensorID == EXTRA_LED1_ID) {
      // set led status 
      digitalWrite(EXTRA_LED1_PIN, newStatus);
    }
    if (sensorID == EXTRA_LED2_ID) {
      // set led status 
      digitalWrite(EXTRA_LED2_PIN, newStatus);
    }*/
    // Store state in eeprom
    saveState(sensorID, newStatus);    
}

char buf[25];

void loop() {
  // put your main code here, to run repeatedly:



  checkTouchSensor();

    for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
       if((millis() - lastSwitchLight) >= 2000 && changedStates[i] == true) {
          send(msg.setSensor(i+1).set(channelState[i]));
           changedStates[i]  = false;    
        }
        if((millis() - lastSwitchLight) >= 2300 && trigger == true) {
          send(msgdebug.setSensor(3).set(buf));
          trigger  = false;    
        }                  
        if((millis() - lastOnCde[i] ) >= MODE_TIMER_MS && channelState[i]== ON && mode[i] == MODE_TIMER) {
          switchLight(i+1, OFF);
        }
    }
      
  /*for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    // Test input pins, connected to analog inputs so we use analog read, 
    //  if > to half of max value we consider it a HIGH value, else LOW
    bool pinValue = (analogRead(buttonPins[i]) > 512);
    if (pinValue != buttonStates[i]) {
      buttonLastChange[i] = millis();
      buttonStates[i] = pinValue;
      if (pinValue) {
        #ifdef MY_DEBUG
           Serial.print("Touch on button ");
           Serial.println(i+1);
        #endif
        // if user just pressed on a button, we change the status of the button/switch
        buttonStates[i] = !buttonStates[i];
        // then set the relay
        switchLight(i+1, buttonStates[i]);
        // and finally as we made a local change of the switch position, send value to gateway/controller :
        send(msg.setSensor(i+1).set(buttonStates[i]));
      }
      else {
        #ifdef MY_DEBUG
           Serial.print("Release of button ");
           Serial.println(i+1);
        #endif
      }
    }    
  }*/
 delay(10);
}

//uint16_t maxReleased[NUMBER_OF_BUTTONS];

  void checkTouchSensor() {
    static uint32_t lastTouchTimestamp[NUMBER_OF_BUTTONS];
    static uint8_t touchSensorState[NUMBER_OF_BUTTONS];

  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
        
    if((hwDigitalRead(buttonPins[i]) == LOW) &&
                (touchSensorState[i] != TOUCHED)) {

            // latch in TOUCH state
            touchSensorState[i] = TOUCHED;
            lastTouchTimestamp[i] = millis();
            //snprintf(buf, sizeof buf, "TOUCHED %02d TIME %02d", i, millis() - lastTouchTimestamp[i]);
           // send(msgdebug.setSensor(3).set(buf));        
             
    }

        if((hwDigitalRead(buttonPins[i]) == HIGH) &&
                (touchSensorState[i] != RELEASED)) {

            lastTouchTimestamp[i] = millis() - lastTouchTimestamp[i];
           // if(lastTouchTimestamp[i] > maxReleased[i]) { maxReleased[i] = lastTouchTimestamp[i];  };
            snprintf(buf, sizeof buf, "RELEASED %02d TIME %02d", i, lastTouchTimestamp[i]);  
            trigger = true;     
            //delay(1000);
           // snprintf(buf, sizeof buf, "MAX %02d RELEASED %02d ", i, maxReleased[i]);
           // send(msgdebug.setSensor(3).set(buf));
            //delay(1000);0
               
            
            // evaluate elapsed time between touch states
            // we can do here short press and long press handling if desired
            if(lastTouchTimestamp[i] >= SHORT_TOUCH_DETECT_THRESHOLD_MS  && lastTouchTimestamp[i] < LONG_TOUCH_DETECT_THRESHOLD_MS) {
                mode[i] = MODE_TIMER;
                buttonStates[i] = !buttonStates[i];
                switchLight(i+1, buttonStates[i]);
            }
            if(lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS) {
              mode[i] = MODE_NORMAL;
              buttonStates[i] = !buttonStates[i];
              switchLight(i+1, buttonStates[i]);
              BipBuzzerFastly(2, BUZZER_PIN);              
            }
            // latch in RELEASED state
            touchSensorState[i] = RELEASED;
            
    }
  }
  
}




void setChannelRelaySwitchState(uint8_t channel, uint8_t newState) {


  
    if(newState == ON) {

        
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], HIGH);
        delay(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], LOW);
        //delay(RELAY_PULSE_DELAY_MS);
        channelState[channel] = ON;
        digitalWrite(ledPins[channel], channelState[channel]);
        //wait(RELAY_PULSE_DELAY_MS*5);
        //TURN_RED_LED_ON(channel);
    } else {
      
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], HIGH);
        delay(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], LOW);
        //delay(RELAY_PULSE_DELAY_MS);
        channelState[channel] = OFF;
        digitalWrite(ledPins[channel], channelState[channel]);
       // delay(3000);
        //TURN_BLUE_LED_ON(channel);
    }

        #ifdef MY_DEBUG
           Serial.print("Channel ");
           Serial.println(channel);
        #endif
}


/**************************************************************************************/
/* Allows to fastly blink the buzzer.                                                    */
/**************************************************************************************/
void BipBuzzerFastly(byte loop, byte pinToBlink)
  {
  byte delayOn = 100;
  byte delayOff = 100;
  for (int i = 0; i < loop; i++)
    {
    blinkLed(pinToBlink, delayOn);
    delay(delayOff);
    }
  }
  
/**************************************************************************************/
/* Allows to fastly blink the LED.                                                    */
/**************************************************************************************/
void blinkLedFastly(byte loop, byte pinToBlink)
  {
  byte delayOn = 150;
  byte delayOff = 150;
  for (int i = 0; i < loop; i++)
    {
    blinkLed(pinToBlink, delayOn);
    delay(delayOff);
    }
  }


/**************************************************************************************/
/* Allows to blink a LED.                                                             */
/**************************************************************************************/
void blinkLed(byte pinToBlink, int delayInMs)
  {
  digitalWrite(pinToBlink,HIGH);
  delay(delayInMs);
  digitalWrite(pinToBlink,LOW);
  }
  

