//Fuse L:E2 H:DA E:05 
//// Enable and select radio type attached
//#define MY_RADIO_NRF24
#define MY_RADIO_RFM69
//#define MY_IS_RFM69HW
//#define MY_RFM69_MAX_POWER_LEVEL_DBM (0u)
//#define MY_RFM69_NEW_DRIVER

//#define MY_RFM69_FREQUENCY RF69_868MHZ


#define MY_DEBUG
//#define MY_OTA_FIRMWARE_FEATURE  // Enables OTA firmware updates if DualOptiBoot

#define MY_NODE_ID 2
//#define MY_TRANSPORT_WAIT_READY_MS 1000
/*#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC*/
#include <MySensors.h>

#define NUMBER_OF_BUTTONS 2 // Total number of attached relays
//#define EXTRA_LED1_ID 11
//#define EXTRA_LED2_ID 12
//#define EXTRA_LED1_PIN A4
//#define EXTRA_LED2_PIN A5

int ledPins[] = {A1,5};
byte buttonPins[] = {A0,4};
byte relayPins[] = {9,3,6,7};
const uint8_t RELAY_CH_PINS[][2] = {
    {9, 3}, // channel 1 relay control pins(bistable relay - 2 coils)
    {6, 7}  // channel 2 relay control pins(bistable relay - 2 coils)
};
const uint32_t RELAY_PULSE_DELAY_MS = 40; //Def 50

#define OFF 0
#define ON  1
#define SET_COIL_INDEX     0
#define RESET_COIL_INDEX   1

#define RELEASED  0
#define TOUCHED   1
const uint32_t SHORT_TOUCH_DETECT_THRESHOLD_MS = 20 ; 
const uint32_t LONG_TOUCH_DETECT_THRESHOLD_MS = 2000;

bool buttonStates[] = {false, false};
bool touchStates[] = {false, false};
long buttonLastChange[] = {0,0};
uint8_t channelState[] = {OFF, OFF};
bool changedStates[] = {false, false};
bool test = false;
byte switchOnCommands[] = {8,24,136};
byte switchOffCommands[] = {16,128,144};
uint32_t lastSwitchLight = -1;
uint32_t lastReceivCom = -1;
#define RESET_PORT_COMMAND 103




MyMessage msg(1, V_TRIPPED);


void before()
{

   pinMode(A2, OUTPUT); //RST
   
  // initialize led, relays pins as outputs and buttons as inputs
  for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    pinMode(ledPins[i], OUTPUT);
    blinkLedFastly(3, ledPins[i]);
    digitalWrite(ledPins[i], buttonStates[i]);
    pinMode(buttonPins[i], INPUT);
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

 
  sendSketchInfo("LivoloRelay2", "0.54");

  // Register sensors to gateway
  for (int j = 0; j < NUMBER_OF_BUTTONS; j++) {
    present(j+1, S_BINARY);
  }

 
 // present(EXTRA_LED1_ID, S_BINARY);
 // present(EXTRA_LED2_ID, S_BINARY);

  //sleep(5000);
//  for (int l = 1; l <= NUMBER_OF_BUTTONS; l++) {
//    switchLight(l, loadState(l));
//  }
//  switchLight(EXTRA_LED1_ID, loadState(EXTRA_LED1_ID));
//  switchLight(EXTRA_LED2_ID, loadState(EXTRA_LED2_ID));
}
/*
void receive(const MyMessage &message)
{

  if(millis() > 10000){
    lastReceivCom = millis();
    // We only expect one type of message from controller. But we better check anyway.
   if (message.type==V_STATUS && test == false) {
      if (message.sensor<=NUMBER_OF_BUTTONS) {
        switchLight(message.sensor, message.getBool());
        buttonStates[message.sensor-1] = message.getBool();
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
}*/

/// Switch off relay
/// relayPosition: position of relay, from 1 to 3
void switchRelay(int relayPosition, bool newStatus) {
   // Physically set the relay status by writing the value directly to the port
   // We first make a "AND" operation with the "reset" value which sets the 3 bits managing the relays to 0
   //  then we make a "OR" with the byte setting the right bits for the relays we want to switch on/off
#ifdef MY_DEBUG
   Serial.print("PORTD BEFORE SWITCH :");
   Serial.println(PORTD,BIN);
#endif
   //PORTD = (PORTD & RESET_PORT_COMMAND) | (newStatus?switchOnCommands[relayPosition-1]:switchOffCommands[relayPosition-1]);
#ifdef MY_DEBUG
   Serial.print("PORTD AFTER SWITCH :");
   Serial.println(PORTD,BIN);
#endif
}

/// Change status of "switch" or extra led
void switchLight(int sensorID, bool newStatus) {
    // if this is a physical relay, we need to switch on the relay
    if (sensorID<=NUMBER_OF_BUTTONS) {
        setChannelRelaySwitchState(sensorID-1, newStatus);
        lastSwitchLight = millis();
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


void loop() {
  // put your main code here, to run repeatedly:


/*while(digitalRead(A3) == LOW){
  wait(5);
}*/


//switchLight(1, true);
//send(msg.setSensor(1).set(true));

//switchLight(1, false);
//send(msg.setSensor(1).set(true));

  checkTouchSensor();

   /* for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
       if((millis() - lastSwitchLight) >= 1000 && changedStates[i] == true) {
          send(msg.setSensor(i+1).set(channelState[i]));
           //wait(1000);
           changedStates[i]  = false;    
        }
    }*/
      
  /*for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    // Test input pins, connected to analog inputs so we use analog read, 
    //  if > to half of max value we consider it a HIGH value, else LOW
    bool pinValue = (analogRead(buttonPins[i]) > 512);
    if (pinValue != touchStates[i]) {
      buttonLastChange[i] = millis();
      touchStates[i] = pinValue;
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
 // wait(10);
}

  void checkTouchSensor() {
    static uint32_t lastTouchTimestamp[NUMBER_OF_BUTTONS];
    static uint8_t touchSensorState[NUMBER_OF_BUTTONS];

  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
        
    if((hwDigitalRead(buttonPins[i]) == HIGH) &&
                (touchSensorState[i] != TOUCHED)) {

            // latch in TOUCH state
            touchSensorState[i] = TOUCHED;
            lastTouchTimestamp[i] = millis();
    }

        if((hwDigitalRead(buttonPins[i]) == LOW) &&
                (touchSensorState[i] != RELEASED)) {

            lastTouchTimestamp[i] = millis() - lastTouchTimestamp[i];
            // evaluate elapsed time between touch states
            // we can do here short press and long press handling if desired
            if(lastTouchTimestamp[i] >= SHORT_TOUCH_DETECT_THRESHOLD_MS /*&& lastTouchTimestamp[i] < LONG_TOUCH_DETECT_THRESHOLD_MS*/) {
                touchStates[i] = !touchStates[i];
                switchLight(i+1, touchStates[i]);
            }
            /*if(lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS) {
              blinkLedFastly(6, ledPins[i]);
            }*/
            // latch in RELEASED state
            touchSensorState[i] = RELEASED;
    }
  }
  
}




void setChannelRelaySwitchState(uint8_t channel, uint8_t newState) {

if(test != true) {
test = true;
  
    if(newState == ON) {

        
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], HIGH);
        wait(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], LOW);
        wait(RELAY_PULSE_DELAY_MS);
        channelState[channel] = ON;
        digitalWrite(ledPins[channel], channelState[channel]);
        //wait(RELAY_PULSE_DELAY_MS*5);
        //TURN_RED_LED_ON(channel);
    } else {
      
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], HIGH);
        wait(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], LOW);
        wait(RELAY_PULSE_DELAY_MS);
        channelState[channel] = OFF;
        digitalWrite(ledPins[channel], channelState[channel]);
       // wait(RELAY_PULSE_DELAY_MS*5);
        //TURN_BLUE_LED_ON(channel);
    }

    test = false;
        #ifdef MY_DEBUG
           Serial.print("Channel ");
           Serial.println(channel);
        #endif
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
  

