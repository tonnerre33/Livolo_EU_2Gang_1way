//Fuse L:E2 H:DA E:05 
//// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69


//#define MY_DEBUG
//#define MY_OTA_FIRMWARE_FEATURE  // Enables OTA firmware updates if DualOptiBoot

//#define MY_NODE_ID 3
//#define MY_DISABLED_SERIAL
//#define MY_TRANSPORT_WAIT_READY_MS 1000
/*#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC*/
#include <MySensors.h>

#define NUMBER_OF_BUTTONS 2 // Total number of attached relays


int ledPins[] = {7,A1}; //{LED BP LEFT, LED BP RIGHT}
byte buttonPins[] = {3,A0}; //{BP LEFT, BP RIGHT}
const uint8_t RELAY_CH_PINS[][2] = {
    {5, 4}, // channel 1 relay control pins(bistable relay - 2 coils) {L1_SET, L1_RST}
    {A3, A4}  // channel 2 relay control pins(bistable relay - 2 coils) {L2_SET, L2_RST}
};
#define MTSA_PIN 6
#define BUZZER_PIN A2


const uint32_t RELAY_PULSE_DELAY_MS = 40; //Def 50

#define OFF 0
#define ON  1
#define SET_COIL_INDEX     0
#define RESET_COIL_INDEX   1
#define RELEASED  0
#define TOUCHED   1
const uint32_t SHORT_TOUCH_DETECT_THRESHOLD_MS = 50; 
const uint32_t LONG_TOUCH_DETECT_THRESHOLD_MS = 500;
const uint32_t MODE_TIMER_MS = 60000;
const uint32_t SUCCESSIVE_SENSOR_DATA_SEND_DELAY_MS = 100;
const uint8_t BUTTON_SENSIBLITY = 80; 

//bool buttonStates[] = {false, false};
long buttonLastChange[] = {OFF, OFF};
uint8_t channelState[] = {OFF, OFF};
bool changedStates[] = {false, false};
bool trigger = false;
uint32_t lastSwitchLight = -1;
uint32_t lastOnCde[NUMBER_OF_BUTTONS];

#define MODE_NORMAL 0
#define MODE_TIMER 1
#define MODE_INSTANT 2

uint8_t MODE_BUTTON[] { MODE_INSTANT, MODE_INSTANT};

uint8_t mode[] = {MODE_NORMAL, MODE_NORMAL};
const uint8_t SENSOR_DATA_SEND_RETRIES = 3;
const uint32_t SENSOR_DATA_SEND_RETRIES_MIN_INTERVAL_MS = 10;
const uint32_t SENSOR_DATA_SEND_RETRIES_MAX_INTERVAL_MS = 50;


MyMessage msgdebug(1, V_TEXT);


void before()
{
    wdt_disable();
    wdt_enable(WDTO_8S);

   
  // initialize led, relays pins as outputs and buttons as inputs
  for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
    //SENSIBILITY
    pinMode(MTSA_PIN, OUTPUT);
    analogWrite(MTSA_PIN, 255 - BUTTON_SENSIBLITY * 255/100);

    //BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    //LEDS
    pinMode(ledPins[i], OUTPUT);
    blinkLedFastly(3, ledPins[i]);
    digitalWrite(ledPins[i],  loadState(i+1));
    //BUTTONS
    pinMode(buttonPins[i], INPUT);
    digitalWrite(buttonPins[i], HIGH);       // turn on pullup resistors
   //RELAYS
    for (int j = 0; j < NUMBER_OF_BUTTONS; j++) {
      pinMode(RELAY_CH_PINS[i][j], OUTPUT);      
    }
    //LOAD PREVIOUS STATE
    channelState[i] = loadState(i+1);

      

  }


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
 
}

void receive(const MyMessage &message)
{




switch (message.type) {

        case V_STATUS:
            // V_STATUS message type for light switch set operations only
            if (message.getCommand() == C_SET) {
                // maybe perform some received data validation here ???
                switchLight((message.sensor), message.getBool());
            }

            // V_STATUS message type for light switch get operations only
            if(message.getCommand() == C_REQ) {
                // maybe perform some received data validation here ???
                sendData(message.sensor, getChannelState(message.sensor - 1), V_STATUS);
            }
            break;
        default:;
    }  

  
}

/// Change status of "switch"
void switchLight(int sensorID, bool newStatus) {
    // if this is a physical relay, we need to switch on the relay
    if (sensorID<=NUMBER_OF_BUTTONS) {
        setChannelRelaySwitchState(sensorID-1, newStatus);
        lastSwitchLight = millis();
        if(newStatus == ON) { lastOnCde[sensorID-1] = millis();  } //channelState[sensorID-1] = true;
        //else { channelState[sensorID-1] = false; }
        changedStates[sensorID-1]  = true;    
    }


    // Store state in eeprom
    saveState(sensorID, newStatus);    
}

char buf[25];

void loop() {
  // put your main code here, to run repeatedly:
  
  wdt_reset(); //WatchDog

  checkTouchSensor();

    for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
       if((millis() - lastSwitchLight) >= 2000 && changedStates[i] == true) {
           sendData(i+1, channelState[i], V_STATUS);
           //send(msg.setSensor(i+1).set(channelState[i]));
           changedStates[i]  = false;    
        }
        
        if((millis() - lastSwitchLight) >= 2300 && trigger == true && changedStates[i] == false) {
          //sendData(3, buf, V_TEXT);
          send(msgdebug.setSensor(3).set(buf));
          trigger  = false;    
        }        
                         
        if((millis() - lastOnCde[i] ) >= MODE_TIMER_MS && channelState[i]== ON && mode[i] == MODE_TIMER) {
          switchLight(i+1, OFF);
        }
    }
      
}

  void checkTouchSensor() {
    static uint32_t lastTouchTimestamp[NUMBER_OF_BUTTONS];
    static uint8_t touchSensorState[NUMBER_OF_BUTTONS];

  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
        
    if((hwDigitalRead(buttonPins[i]) == LOW) &&
                (touchSensorState[i] != TOUCHED)) { 

            if(MODE_BUTTON[i] == MODE_INSTANT){
                      mode[i] = MODE_TIMER;
                      channelState[i] = !channelState[i];
                      switchLight(i+1, channelState[i]);
            }    
                        
            // latch in TOUCH state
            touchSensorState[i] = TOUCHED;
            lastTouchTimestamp[i] = millis();
            //snprintf(buf, sizeof buf, "TOUCHED %02d TIME %02d", i, millis() - lastTouchTimestamp[i]);
           // send(msgdebug.setSensor(3).set(buf));              
    }


      
       if((hwDigitalRead(buttonPins[i]) == HIGH) &&
                    (touchSensorState[i] != RELEASED)) {
                      
                lastTouchTimestamp[i] = millis() - lastTouchTimestamp[i];
                snprintf(buf, sizeof buf, "RELEASED %02d TIME %02d", i, lastTouchTimestamp[i]);  
                trigger = true;     
               if(MODE_BUTTON[i] != MODE_INSTANT){  
                  // evaluate elapsed time between touch states
                  // we can do here short press and long press handling if desired
                  if(lastTouchTimestamp[i] >= SHORT_TOUCH_DETECT_THRESHOLD_MS  && lastTouchTimestamp[i] < LONG_TOUCH_DETECT_THRESHOLD_MS) {
                      mode[i] = MODE_TIMER;
                      channelState[i] = !channelState[i];
                      switchLight(i+1, channelState[i]);
                  }
       
                  if(lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS) {
                       mode[i] = MODE_NORMAL;
                      channelState[i] = ON;
                      switchLight(i+1, channelState[i]);
                      BipBuzzerFastly(2, BUZZER_PIN);              
                  }

                }
                // latch in RELEASED state
                touchSensorState[i] = RELEASED;        
      }

    if(MODE_BUTTON[i] == MODE_INSTANT) {
      if((hwDigitalRead(buttonPins[i]) == LOW) && 
          touchSensorState[i] != RELEASED){
                if(millis() - lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS && mode[i] != MODE_NORMAL 
                  && channelState[i] == ON) { // No buzzer on the sleep power
                    mode[i] = MODE_NORMAL;
                    BipBuzzerFastly(2, BUZZER_PIN);              
                }                     
        }

     }
                       
    }
  }
  

uint8_t getChannelState(uint8_t index) {
    return channelState[index];
}


void setChannelRelaySwitchState(uint8_t channel, uint8_t newState) {

    if(newState == ON) {
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], HIGH);
        delay(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], LOW);
        //delay(RELAY_PULSE_DELAY_MS);
        channelState[channel] = ON;
        digitalWrite(ledPins[channel], channelState[channel]);
    } else {
      
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], HIGH);
        delay(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], LOW);
        //delay(RELAY_PULSE_DELAY_MS);
        channelState[channel] = OFF;
        digitalWrite(ledPins[channel], channelState[channel]);
    }

        #ifdef MY_DEBUG
           Serial.print("Channel ");
           Serial.println(channel);
        #endif
}


void sendData(uint8_t sensorId, uint8_t sensorData, uint8_t dataType) {
    MyMessage sensorDataMsg(sensorId, dataType);

    for (uint8_t retries = 0; !send(sensorDataMsg.set(sensorData), false) &&
         (retries < SENSOR_DATA_SEND_RETRIES); ++retries) {
        // random wait interval between retries for collisions
        wait(random(SENSOR_DATA_SEND_RETRIES_MIN_INTERVAL_MS,
            SENSOR_DATA_SEND_RETRIES_MAX_INTERVAL_MS));
    }
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
  

