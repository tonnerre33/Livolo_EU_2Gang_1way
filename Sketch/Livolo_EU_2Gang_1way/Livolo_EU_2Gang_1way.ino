//Fuse L:E2 H:DA E:05
//// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_PA_LEVEL (RF24_PA_LOW)									  
//#define MY_RADIO_RFM69


//#define MY_DEBUG
//#define MY_OTA_FIRMWARE_FEATURE  // Enables OTA firmware updates if DualOptiBoot

//#define MY_NODE_ID 120
//#define MY_DISABLED_SERIAL
#define MY_TRANSPORT_WAIT_READY_MS 10000
/*#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC*/
#include <MySensors.h>
#include <EEPROM.h>


#define NUMBER_OF_BUTTONS 2 // Total number of attached relays

// ID of the settings block
#define CONFIG_VERSION "002"

// Tell it where to store your config data in EEPROM
// mysensors api uses eeprom addresses including 512 so we pick 514 for safety
#define CONFIG_START 514

// Example settings structure
struct StoreStruct {
    // The variables of your settings
    uint8_t button_sensiblity;
    uint16_t mode_timer_s[NUMBER_OF_BUTTONS];
    uint8_t button_mode[NUMBER_OF_BUTTONS];
    uint8_t mode_def[NUMBER_OF_BUTTONS];
    // This is for mere detection if they are your settings
    char version_of_program[4]; // it is the last variable of the struct
    // so when settings are saved, they will only be validated if
    // they are stored completely.
} settings = {
    // The default values
    76, //button_sensiblity
    {60, 60}, //auto off lamp {left, right} seconds
    {1, 1}, // mode button {left, right} 0=MODE_EVENT_TRIGGER, 1=MODE_EVENT_INSTANT
    {1, 1}, // mode by default {left, right} 0=MODE_NORMAL 0, 1=MODE_TIMER, 2=MODE_CLEANING
    CONFIG_VERSION
};


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
const uint32_t LONG_TOUCH_DETECT_CLEANING_MS = 3000;
const uint32_t MODE_CLEANING_MS = 60000;
const uint32_t SUCCESSIVE_SENSOR_DATA_SEND_DELAY_MS = 100;


//bool buttonStates[] = {false, false};
long buttonLastChange[] = {OFF, OFF};
uint8_t channelState[] = {OFF, OFF};
bool changedStates[] = {false, false};
bool trigger[] = {false, false};
uint32_t lastSwitchLight = -1;
uint32_t lastOnCde[NUMBER_OF_BUTTONS];
uint32_t lastMode;

#define MODE_EVENT_TRIGGER 0
#define MODE_EVENT_INSTANT 1

#define MODE_NORMAL 0
#define MODE_TIMER 1
#define MODE_CLEANING 2


const uint8_t SENSOR_DATA_SEND_RETRIES = 3;
const uint32_t SENSOR_DATA_SEND_RETRIES_MIN_INTERVAL_MS = 10;
const uint32_t SENSOR_DATA_SEND_RETRIES_MAX_INTERVAL_MS = 50;

uint8_t MODE[] = {settings.mode_def[0], settings.mode_def[1]};

//MyMessage msgdebug(4, V_TEXT);

void loadConfig() {
    // To make sure there are settings, and they are YOURS!
    // If nothing is found it will use the default settings.
    if (//EEPROM.read(CONFIG_START + sizeof(settings) - 1) == settings.version_of_program[3] // this is '\0'
        EEPROM.read(CONFIG_START + sizeof(settings) - 2) == settings.version_of_program[2] &&
        EEPROM.read(CONFIG_START + sizeof(settings) - 3) == settings.version_of_program[1] &&
        EEPROM.read(CONFIG_START + sizeof(settings) - 4) == settings.version_of_program[0])
    {   // reads settings from EEPROM
        for (unsigned int t=0; t<sizeof(settings); t++)
            *((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
    } else {
        // settings aren't valid! will overwrite with default settings
        saveConfig();
    }
}

void saveConfig() {
    for (unsigned int t=0; t<sizeof(settings); t++)
    {   // writes to EEPROM
        EEPROM.write(CONFIG_START + t, *((char*)&settings + t));
        // and verifies the data
        if (EEPROM.read(CONFIG_START + t) != *((char*)&settings + t))
        {
            // error writing to EEPROM
        }
    }
}

void before()
{
    wdt_disable();
    wdt_enable(WDTO_8S);

    loadConfig(); 
    // initialize led, relays pins as outputs and buttons as inputs

    //SENSIBILITY
    pinMode(MTSA_PIN, OUTPUT);
    analogWrite(MTSA_PIN, 255 - settings.button_sensiblity * 255/100);
    //BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
        //LEDS
        pinMode(ledPins[i], OUTPUT);
        //BUTTONS
        pinMode(buttonPins[i], INPUT);
        digitalWrite(buttonPins[i], HIGH);       // turn on pullup resistors
        //RELAYS
        for (int j = 0; j < 2; j++) {
            pinMode(RELAY_CH_PINS[i][j], OUTPUT);
        }
		
        //LOAD PREVIOUS STATE
        digitalWrite(ledPins[i],  loadState(i+1));
        channelState[i] = loadState(i+1);
    }


}

void presentation()
{
    // Send the sketch version information to the gateway and Controller
    sendSketchInfo("LivoloRelay2", "1.84");

    // Register sensors to gateway
    for (int j = 0; j < NUMBER_OF_BUTTONS; j++) {
        present(j+1, S_BINARY);
        wait(SUCCESSIVE_SENSOR_DATA_SEND_DELAY_MS);
        blinkNumberOutput(3,ledPins[j],150 ,150);
    }
    present(3, S_INFO);
    wait(SUCCESSIVE_SENSOR_DATA_SEND_DELAY_MS);

}

boolean newSettings = false;
void receive(const MyMessage &message)
{


    const byte numChars = 32;
    char receivedChars[numChars];   // an array to store the received data

    switch (message.type) {

    case V_STATUS:
        // V_STATUS message type for light switch set operations only
        if (message.getCommand() == C_SET &&
            message.getBool() != getChannelState(message.sensor - 1)) {
            switchLight((message.sensor), message.getBool());
        }

        // V_STATUS message type for light switch get operations only
        /* if(message.getCommand() == C_REQ) {
          sendData(message.sensor, getChannelState(message.sensor - 1), V_STATUS);
          }*/
        break;
    case V_VAR1:
        if (message.getCommand() == C_SET) {        
            strncpy(receivedChars, message.getString(), numChars);
            settingsUpdate(&receivedChars[0]);   
        }
        break;
    default:
        ;
    }
}

/// Change status of "switch"
void switchLight(int sensorID, bool newStatus) {
    // if this is a physical relay, we need to switch on the relay
    if (sensorID<=NUMBER_OF_BUTTONS) {
        setChannelRelaySwitchState(sensorID-1, newStatus);
        lastSwitchLight = millis();
        if(newStatus == ON) {
            lastOnCde[sensorID-1] = millis();     //channelState[sensorID-1] = true;
        }
        //else { channelState[sensorID-1] = false; }
        changedStates[sensorID-1]  = true;
    }


    // Store state in eeprom
    saveState(sensorID, newStatus);
}

char buf[25];
uint32_t lastTouchTimestamp[NUMBER_OF_BUTTONS];

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

        if((millis() - lastSwitchLight) >= 3000 && trigger[i] == true && changedStates[i] == false) {
            //send(msgdebug.setSensor(4).set(buf));
            sendData(3, lastTouchTimestamp[i], V_TEXT);
            //send(msgdebug.setSensor(3).set(lastTouchTimestamp[i]));
            trigger[i] = false;
        }

        if((millis() - lastOnCde[i] ) >= settings.mode_timer_s[i]*1000 && channelState[i]== ON && MODE[i] == MODE_TIMER) {
            switchLight(i+1, OFF);
        }
        if((millis() - lastMode) >= MODE_CLEANING_MS && MODE[i] == MODE_CLEANING) {
            MODE[0] = settings.mode_def[0];
            MODE[1] = settings.mode_def[1];
            blinkNumberOutput(1, BUZZER_PIN,100,100);
             for (int i = 0; i < NUMBER_OF_BUTTONS; i++) {
                digitalWrite(ledPins[i],  getChannelState(i));
             }
        }
        if(MODE[i] == MODE_CLEANING && i==0){
          blinkEvery(ledPins, 1000,1000);
          wait(10);     //Fix blink in cleaning mode 
        }
    }
}

void checkTouchSensor() {
    static uint8_t touchSensorState[NUMBER_OF_BUTTONS];
    static bool EndWaitFalseTrigger[NUMBER_OF_BUTTONS] = {false, false};

    for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {

        //TOUCHED BUTTON
        if((hwDigitalRead(buttonPins[i]) == LOW) && (touchSensorState[i] != TOUCHED)) {

            // latch in TOUCH state
            touchSensorState[i] = TOUCHED;
            lastTouchTimestamp[i] = millis();
            EndWaitFalseTrigger[i] = false;
            //snprintf(buf, sizeof buf, "TOUCHED %02d TIME %02d", i, millis() - lastTouchTimestamp[i]);
            // send(msgdebug.setSensor(4).set(buf));
        }


        //MAINTAINED TOUCHED BUTTON
        if((hwDigitalRead(buttonPins[i]) == LOW) && (touchSensorState[i] == TOUCHED) &&   EndWaitFalseTrigger[i] == false) {
            // MODE_EVENT_INSTANT
            if(settings.button_mode[i] == MODE_EVENT_INSTANT) {
                if(millis() - lastTouchTimestamp[i] > SHORT_TOUCH_DETECT_THRESHOLD_MS) { //Anti false trigger
                    if(MODE[i] != MODE_CLEANING) {
                        MODE[i] = settings.mode_def[i];
                        channelState[i] = !channelState[i];
                        switchLight(i+1, channelState[i]);
                        EndWaitFalseTrigger[i] = true;
                    } else {
                        blinkNumberOutput(1, BUZZER_PIN,100,100);
                    }
                }
            }

        }


        //RELEASED BUTTON
        if((hwDigitalRead(buttonPins[i]) == HIGH) &&
                (touchSensorState[i] != RELEASED)) {

            lastTouchTimestamp[i] = millis() - lastTouchTimestamp[i];
            snprintf(buf, sizeof buf, "RELEASED %02d TIME %02d", i, lastTouchTimestamp[i]);
            //send(msgdebug.setSensor(3).set(lastTouchTimestamp[i]));
            //wait(SUCCESSIVE_SENSOR_DATA_SEND_DELAY_MS);
            trigger[i] = true;

            if(MODE[i] != MODE_CLEANING) {
                //MODE_EVENT_TRIGGER
                if(settings.button_mode[i] != MODE_EVENT_INSTANT) {
                    // evaluate elapsed time between touch states
                    // we can do here short press and long press handling if desired
                    if(lastTouchTimestamp[i] >= SHORT_TOUCH_DETECT_THRESHOLD_MS  && lastTouchTimestamp[i] < LONG_TOUCH_DETECT_THRESHOLD_MS) {
                        MODE[i] = settings.mode_def[i];
                        channelState[i] = !channelState[i];
                        switchLight(i+1, channelState[i]);
                    }

                    if(lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS && lastTouchTimestamp[i] < LONG_TOUCH_DETECT_CLEANING_MS) {
                        MODE[i] = MODE_NORMAL;
                        channelState[i] = ON;
                        switchLight(i+1, channelState[i]); // No buzzer on the sleep power
                        blinkNumberOutput(2, BUZZER_PIN,100,100);
                    }
                    if(lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_CLEANING_MS) {
                        MODE[0] = MODE_CLEANING;
                        MODE[1] = MODE_CLEANING;
                        lastMode = millis();
                        channelState[i] = ON;
                        switchLight(i+1, channelState[i]); // No buzzer on the sleep power 
                        blinkNumberOutput(1, BUZZER_PIN,100,100);
                    }
                }
            }
            // latch in RELEASED state
            touchSensorState[i] = RELEASED;
        }

        if(settings.button_mode[i] == MODE_EVENT_INSTANT && MODE[i] != MODE_CLEANING) {
            if((hwDigitalRead(buttonPins[i]) == LOW) &&
                    touchSensorState[i] != RELEASED) {
                if(millis() - lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_THRESHOLD_MS && MODE[i] != MODE_NORMAL
                        && channelState[i] == ON) { // No buzzer on the sleep power
                    MODE[i] = MODE_NORMAL;
                    blinkNumberOutput(2, BUZZER_PIN,100,100);
                }
                if(millis() - lastTouchTimestamp[i] >= LONG_TOUCH_DETECT_CLEANING_MS && MODE[i] != MODE_CLEANING
                        && channelState[i] == ON) { // No buzzer on the sleep power
                    MODE[0] = MODE_CLEANING;
                    MODE[1] = MODE_CLEANING;
                    lastMode = millis();
                    blinkNumberOutput(1, BUZZER_PIN,100,100);
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
        wait(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][SET_COIL_INDEX], LOW);
        channelState[channel] = ON;
        digitalWrite(ledPins[channel], channelState[channel]);

    } else {

        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], HIGH);
        wait(RELAY_PULSE_DELAY_MS);
        hwDigitalWrite(RELAY_CH_PINS[channel][RESET_COIL_INDEX], LOW);
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
    for (uint8_t retries = 0; !send(sensorDataMsg.set(sensorData), true) &&
            (retries < SENSOR_DATA_SEND_RETRIES); ++retries) {
        // random wait interval between retries for collisions
        wait(random(SENSOR_DATA_SEND_RETRIES_MIN_INTERVAL_MS,
                    SENSOR_DATA_SEND_RETRIES_MAX_INTERVAL_MS));
    }
}
void sendData(uint8_t sensorId, String sensorData, uint8_t dataType) {
    MyMessage sensorDataMsg(sensorId, dataType);
    for (uint8_t retries = 0; !send(sensorDataMsg.set(sensorData.c_str()), true) &&
            (retries < SENSOR_DATA_SEND_RETRIES); ++retries) {
        // random wait interval between retries for collisions
        wait(random(SENSOR_DATA_SEND_RETRIES_MIN_INTERVAL_MS,
                    SENSOR_DATA_SEND_RETRIES_MAX_INTERVAL_MS));
    }
}


/**************************************************************************************/
/* Allows to fastly blink an output.                                                  */
/**************************************************************************************/
//Don't use wait() before init radio (void before)
void blinkNumberOutput(byte loop, byte pinToBlink, int delayOnInMs, int delayOffInMs)
{
    for (int i = 0; i < loop; i++)
    {
        digitalWrite(pinToBlink,HIGH);
        wait(delayOnInMs);
        digitalWrite(pinToBlink,LOW);
        wait(delayOffInMs);
    }
}
/**************************************************************************************/
/* Blink output every xx ms                                                           */
/**************************************************************************************/
//Don't use wait() before init radio (void before)
bool changeBlinkState = false;
void blinkEvery(int pinToBlink[], int delayOnInMs, int delayOffInMs)
{
  static uint32_t lastBlinkTimestamp;
  
  for (int i = 0; i < sizeof(pinToBlink); i++) {
      if(millis() - lastBlinkTimestamp > delayOnInMs && digitalRead(pinToBlink[i]) ){

            digitalWrite(pinToBlink[i],LOW);
            changeBlinkState = true;
            continue;
      }
      if(millis() - lastBlinkTimestamp > delayOffInMs && !digitalRead(pinToBlink[i]) ){
            digitalWrite(pinToBlink[i],HIGH);
            changeBlinkState = true;
      }
  }
  if(changeBlinkState){       
    lastBlinkTimestamp = millis();
    changeBlinkState = false;
  }
}

void settingsUpdate(char *pReceveivedChars){

char paramName[20];
char paramValue[10];
char buf[25];

    strncpy(paramName, strtok(pReceveivedChars, "="),20);
    strncpy(paramValue, strtok(NULL, "="),10);
    
      if(strcmp(paramName, "button_sensiblity") == 0) {
         updateVariable(&settings.button_sensiblity, paramValue);
      }

      char paramValueTmp[10];
      for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
        if(i==0){strncpy(paramValueTmp, strtok(paramValue, ","),10);}
        else{strncpy(paramValueTmp, strtok(NULL, ","),10);}

         if(strcmp(paramName,"mode_timer_s") == 0) {
           updateVariable(&settings.mode_timer_s[i], paramValueTmp);
         }
         if(strcmp(paramName,"button_mode") == 0) {
           updateVariable(&settings.button_mode[i], paramValueTmp);
         }        
          
      }    
      
    if(strcmp(pReceveivedChars, "save_settings") == 0){
      if(newSettings == true){
        saveConfig();
        sendData(4, "config_saved", V_TEXT); //No spaces in the text cause crash the gateway in mysensors 2.2.0-rc.1
       newSettings = false;
      }else{
        sendData(4, "no_changes", V_TEXT);
      }
    }
 
    if(strcmp(pReceveivedChars, "print_settings") == 0){

       printSetting(&settings.button_sensiblity, 0, "button_sensiblity","%d");
       printSetting(&settings.mode_timer_s, NUMBER_OF_BUTTONS, "mode_timer_s","%d");
       printSetting(&settings.button_mode, NUMBER_OF_BUTTONS, "button_mode","%d");
        
    }

}

// Generic catch-all implementation.
template <typename T_ty> struct TypeInfo { static const char * name; };
template <typename T_ty> const char * TypeInfo<T_ty>::name = "unknown";

// Handy macro to make querying stuff easier.
#define TYPE_NAME(var) TypeInfo< typeof(var) >::name

// Handy macro to make defining stuff easier.
#define MAKE_TYPE_INFO(type)  template <> const char * TypeInfo<type>::name = #type;

// Type-specific implementations.
MAKE_TYPE_INFO( int )
MAKE_TYPE_INFO( float )
MAKE_TYPE_INFO( short )
MAKE_TYPE_INFO( uint8_t )
MAKE_TYPE_INFO( uint16_t )


template <typename T_type> T_type updateVariable(T_type *pVariable, char value[10]){


      if(strcmp(TYPE_NAME(*pVariable), "uint8_t") == 0){
          *pVariable =  (uint8_t)atoi(value);
      }
      if(strcmp(TYPE_NAME(*pVariable), "uint16_t") == 0){
          *pVariable =  (uint16_t)atoi(value);
      } 
       sendData(4, *pVariable, V_TEXT);
       newSettings = true;
      

}
template <typename T_type> void  printSetting(T_type *pVariable, int numberArray, String variableName, String stringFormat){
  char buf[25];
  String message = variableName + "=" + stringFormat;
  if(numberArray == 0){
        snprintf(buf, sizeof buf, message.c_str() ,*pVariable);
  }else{
       
        for (uint8_t i = 0; i < numberArray; i++) {
         if(strcmp(variableName.c_str(), "mode_timer_s") == 0){
            if(i==0)   {   snprintf(buf, sizeof buf, message.c_str(), *((uint16_t*)pVariable+i));}
            else {snprintf(buf + strlen(buf), 25 - strlen(buf), (","+stringFormat).c_str(),*((uint16_t*)pVariable+i));}
          }
          else{
            if(i==0)   {   snprintf(buf, sizeof buf, message.c_str(), *((uint8_t*)pVariable+i));}
            else {snprintf(buf + strlen(buf), 25 - strlen(buf), (","+stringFormat).c_str(),*((uint8_t*)pVariable+i));}
          }          
        }    
  }

        sendData(4, buf, V_TEXT);

}