#include <Ethernet.h>
#include <EthernetClient.h>

#include <PubSubClient.h>
#include <ATT_IOT.h>                            //AllThingsTalk IoT library
#include <NW_WatchDog.h>
#include <SPI.h>                                //required to have support for signed/unsigned long type..
#include <EEPROM.h>                             //get/store configs
#include <Controllino.h>
#include <avr/wdt.h>							//for watchdog	

#define DEBUG                                   //turn off to remove serial logging so it runs faster and takes up less mem.
//uncomment  if the device needs to recreate all it's assets upon startup (ex: when placed into new account).
//#define CREATEONSTART 					

#include "keys.h"

ATTDevice Device(deviceId, clientId, clientKey);            	//create the object that provides the connection to the cloud to manager the device.
char httpServer[] = "api.smartliving.io";                   	// HTTP API Server host                  
char mqttServer[] = "broker.smartliving.io";            		// MQTT Server Address 
//IPAddress localMqttServer(192, 168, 1, 108);


byte inputs[] = {CONTROLLINO_A0, CONTROLLINO_A1, CONTROLLINO_A2, CONTROLLINO_A3, CONTROLLINO_A4,
                 CONTROLLINO_A5, CONTROLLINO_A6, CONTROLLINO_A7, CONTROLLINO_A8, CONTROLLINO_A9,
                 CONTROLLINO_A10, CONTROLLINO_A11, CONTROLLINO_A12, CONTROLLINO_A13, CONTROLLINO_A14,
                 CONTROLLINO_A15, CONTROLLINO_I16, CONTROLLINO_I17, CONTROLLINO_I18, CONTROLLINO_IN0,
                 CONTROLLINO_IN1};
                 
byte outputs[] = {CONTROLLINO_D0, CONTROLLINO_D1, CONTROLLINO_D2, CONTROLLINO_D3, CONTROLLINO_D4,
                  CONTROLLINO_D5, CONTROLLINO_D6, CONTROLLINO_D7, CONTROLLINO_D8, CONTROLLINO_D9,
                  CONTROLLINO_D10, CONTROLLINO_D11, CONTROLLINO_D12, CONTROLLINO_D13, CONTROLLINO_D14,
                  CONTROLLINO_D15, CONTROLLINO_D16, CONTROLLINO_D17, CONTROLLINO_D18, CONTROLLINO_D19};              
                  
byte relays[] = {CONTROLLINO_R0, CONTROLLINO_R1, CONTROLLINO_R2, CONTROLLINO_R3, CONTROLLINO_R4,
                 CONTROLLINO_R5, CONTROLLINO_R6, CONTROLLINO_R7, CONTROLLINO_R8, CONTROLLINO_R9,
                 CONTROLLINO_R10, CONTROLLINO_R11, CONTROLLINO_R12, CONTROLLINO_R13, CONTROLLINO_R14,
                 CONTROLLINO_R15};                

//sizes of the config parameters
#define IOMAPSIZE 21                                                    
#define PINTYPESIZE 21
#define USEDRELAYSSIZE 16
#define OUTPUTSSIZE 20

//cloud/fog id's for the config parmaters
#define IOMAPID 99
#define PINTYPESID 98
#define USEDRELAYSID 97
#define OUTPUTSID 94
#define APPID 90
//#define ERRORID 95
        
//maps input pins with output pins      
byte ioMap[IOMAPSIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,0xFF , 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char pinTypes[PINTYPESIZE + 1] = "DDDDDDDDDDDDDDDDDDDDD";               								//specify for each input pin if it is analog, digital button or digital toggle, or not used
int prevPinValues[PINTYPESIZE];                                     									//used to keep track of the previous state of the input pins, for analog, it stores the prev value, for digital, it  stores the prev value of the pin .
unsigned short usedRelays = 0xFFFF;                                                   					//bit mask to specify which relays are used or not.
unsigned int usedOutputs = 0xF;                                                        					//bit mask to speivy which digitial outputs are used -> so we know how to activate them.
bool curOutputValues[40] = {false, false, false, false, false, false, false, false, false, false, 		//we use a bigger area, so that the index can correspond to the pin value, makes lookups a lot faster. It does require a little calculation though, cause a part of the range has to be remapped to a smaller number.
							false, false, false, false, false, false, false, false, false, false,
							false, false, false, false, false, false, false, false, false, false, 
							false, false, false, false, false, false, false, false, false, false};

//required for the device
void callback(char* topic, byte* payload, unsigned int length);
EthernetClient ethClient;
PubSubClient pubSub(mqttServer, 1883, callback, ethClient);
NW_WatchDog WatchDog(pubSub, deviceId, clientId);

#define ETHSTARTED 1
#define DEVICECREATED 2
#define SUBSCRIBED 3
int initState = 0;			//keeps track of the current initialization state. so the device can start up as soon as possible and attempt to create network when possible, while already providing core functionality.


//read all the configs from the eeprom.
//configs determine which inputs are connected to which outputs, if Inputs are analog, pushbuttons or toggle buttons.
void readConfigData()
{
    if(EEPROM.read(0) != 255)
    {
        #ifdef DEBUG 
        Serial.println("reading previous config"); 
        Serial.println("io mappings");
        #endif
        for(int i = 0; i < IOMAPSIZE; i++){
            ioMap[i] = EEPROM.read(i + 1);
            #ifdef DEBUG 
            Serial.print(i); Serial.print(" = "); Serial.println(ioMap[i]);
            #endif
        }
        #ifdef DEBUG 
        Serial.println("pin types");
        #endif
        for(int i = 0; i < PINTYPESIZE; i++){
            pinTypes[i] = EEPROM.read(i + 1 + IOMAPSIZE);
            #ifdef DEBUG 
            Serial.print(i); Serial.print(" = "); Serial.println(pinTypes[i]);
            #endif
        }
        byte *pUsedRelays = (byte*)&usedRelays;
        pUsedRelays[0] = EEPROM.read( 1 + IOMAPSIZE + PINTYPESIZE);
        pUsedRelays[1] = EEPROM.read( 2 + IOMAPSIZE + PINTYPESIZE);
        #ifdef DEBUG 
        Serial.print("used relays: "); Serial.println(usedRelays, HEX);
        #endif
        byte *poututs = (byte*)&usedOutputs;
        poututs[0] = EEPROM.read( 3 + IOMAPSIZE + PINTYPESIZE);
        poututs[1] = EEPROM.read( 4 + IOMAPSIZE + PINTYPESIZE);
        poututs[2] = EEPROM.read( 5 + IOMAPSIZE + PINTYPESIZE);
		poututs[3] = EEPROM.read( 6 + IOMAPSIZE + PINTYPESIZE);
        #ifdef DEBUG 
        Serial.print("used digital outputs: "); Serial.println(usedOutputs, HEX);
        #endif
    }
}

//stores the new definition of active outputs into global mem and flash (if changed)
void storeOutputs(unsigned int newValue)
{
    if(newValue != usedOutputs)
    {
        #ifdef DEBUG 
        Serial.print("storing used outputs: "); Serial.println(newValue);
        #endif
        usedOutputs = newValue;
        byte *poututs = (byte*)&usedOutputs;
        EEPROM.write(3 + IOMAPSIZE + PINTYPESIZE, poututs[0]);
        EEPROM.write(4 + IOMAPSIZE + PINTYPESIZE, poututs[1]);
        EEPROM.write(5 + IOMAPSIZE + PINTYPESIZE, poututs[2]);
		EEPROM.write(6 + IOMAPSIZE + PINTYPESIZE, poututs[3]);
        EEPROM.write(0, 1);                                     //indicate that the eeprom has memory stored
    }
}

//stores the new definition of used relays into global mem and flash (if changed)
void storeUsedRelays(unsigned short newValue)
{
    if(newValue != usedRelays)
    {
        #ifdef DEBUG 
        Serial.print("storing used relays: "); Serial.println(newValue);
        #endif
        usedRelays = newValue;
        byte *pUsedRelays = (byte*)&usedRelays;
        EEPROM.write(1 + IOMAPSIZE + PINTYPESIZE, pUsedRelays[0]);
        EEPROM.write(2 + IOMAPSIZE + PINTYPESIZE, pUsedRelays[1]);
        EEPROM.write(0, 1);                                     //indicate that the eeprom has memory stored
    }
}


//checks if the 2 arrays are different, if so, the new array is stored into the new one and into the eeprom
void storePinTypes(const char *newValues)
{
    #ifdef DEBUG 
    Serial.println("storing pintypes");
    #endif
    bool different = false;
    for(int i = 0; i < PINTYPESIZE; i++){
        if(newValues[i] != pinTypes[i]){
            different = true;
            pinTypes[i] = newValues[i];
            #ifdef DEBUG 
            Serial.print(i); Serial.print(" = "); Serial.println(newValues[i]);
            #endif
        }
    }
    if(different){
        #ifdef DEBUG 
        Serial.println("storing pintypes to eeprom");
        #endif
        for(int i = 0; i < PINTYPESIZE; i++){
            EEPROM.write(i + 1 + IOMAPSIZE, pinTypes[i]);
        }
        EEPROM.write(0, 1);                                     //indicate that the eeprom has memory stored
    }
}


bool parseArray(const char *newValues, int* result){
  byte curResult = 0;
  char * ptr;
  if (*newValues != '[') return false;
  newValues++;      										//first char = [
  result[curResult++] = strtol(newValues, &ptr, 10);
  while(*ptr != 0 && *ptr != ']'){
    newValues = ptr + 1;
    result[curResult++] = strtol(newValues, &ptr, 10);
    if(*ptr == ']') return true;                        	//found the end.
  }
  return false;
}


//checks if the 2 arrays are different, if so, the new array is stored into the new one and into the eeprom
bool storeioMap(const char *newValues)
{
    #ifdef DEBUG 
    Serial.println("storing ioMap");
    #endif
	
	int newMap[IOMAPSIZE];
	if (!parseArray(newValues, newMap)) {
		Serial.println("parseArray() failed: can't store iomap");
		return false;
	  }
	
    bool different = false;
    for(int i = 0; i < IOMAPSIZE; i++){
        if(newMap[i] != ioMap[i]){
            different = true;
            ioMap[i] = newMap[i];
            #ifdef DEBUG 
            Serial.print(i); Serial.print(" = "); Serial.println(ioMap[i]);
            #endif
        }
    }
    if(different){
        #ifdef DEBUG 
        Serial.println("storing ioMap to eeprom");
        #endif
        for(int i = 0; i < IOMAPSIZE; i++){
            EEPROM.write(i + 1, ioMap[i]);
        }
        EEPROM.write(0, 1);                                     //indicate that the eeprom has memory stored
    }
	return true;
}


//initialize all the pins according to the configs.
void initPins()
{
    for(int i = 0 ; i < PINTYPESIZE; i++) {
        if(pinTypes[i] == 'A' || pinTypes[i] == 'B' || pinTypes[i] == 'T'){
            #ifdef DEBUG 
            Serial.print("init input pin: "); Serial.println(i);
            #endif
            pinMode(inputs[i], INPUT);
        }
    }
    
    unsigned short relay = 1;
    for(int i = 0 ; i < USEDRELAYSSIZE; i++) {
        if((relay & usedRelays) != 0){
            #ifdef DEBUG 
            Serial.print("init relay: "); Serial.println(i);
            #endif
            pinMode(relays[i], OUTPUT);
        }
        relay = relay << 1;
    }
    
    unsigned int output = 1;
    for(int i = 0 ; i < OUTPUTSSIZE; i++) {
        if((output & usedOutputs) != 0){
            #ifdef DEBUG 
            Serial.print("init output pin: "); Serial.println(i);
            #endif
            pinMode(outputs[i], OUTPUT);
        }
        output = output << 1;
    }
}

//set up the ethernet connection
bool initNetwork()
{
    byte mac[] = {  0x90, 0xA2, 0xDA, 0x0D, 0x8D, 0x3D };       // Adapt to your Arduino MAC Address 
    if (Ethernet.begin(mac, 2000, 1000) == 0)                   // Initialize the Ethernet connection, use a minimum of time, so that the watchdog doesn't keep rebooting at this point if there is no ethernet.
    { 
        #ifdef DEBUG 
        Serial.println(F("DHCP failed"));
        #endif
		delay(100); 
        return false;
    }
    delay(100);                                            		//give the Ethernet shield a second to initialize:
	return true;
}


//prepare the device in the fog/cloud -> declare all the assets and the correct types
bool syncDevice()
{
    if(Device.Connect(&ethClient, httpServer) == true){
		WatchDog.Setup(Device);
		Device.AddAsset(APPID, "application id", "Identify the application.", false, "string");
		Device.AddAsset(IOMAPID, "IO map", "link inputs with outputs.", true, "{\"type\": \"array\", \"items\":{\"type\":\"integer\"}}");   // Create the Digital Actuator asset for your device
		Device.AddAsset(PINTYPESID, "pin types", "specify for each input pin if it is analog (A), button (B), toggle (T) or not used (any other).", true, "string"); 
		Device.AddAsset(USEDRELAYSID, "used relays", "Specify which relays (outputs) are used, as a bitfield (16 bits)", true, "integer"); 
		Device.AddAsset(OUTPUTSID, "used outputs", "Specify which digital outputs are used (20 bits)", true, "integer"); 
		//Device.AddAsset(ERRORID, "last error", "the last error produced by the device (after bootup)", false, "string"); 
		wdt_reset();												//make certain that we don't kill the app with the watchdog
		for(int i = 0; i < PINTYPESIZE; i++){
			String label(i);
			if(pinTypes[i] == 'A')
				Device.AddAsset(inputs[i], "knob " + label, "an analog input pin", false, "integer"); 
			else if(pinTypes[i] == 'B')
				Device.AddAsset(inputs[i], "button " + label, "a push button input pin", false, "boolean"); 
			else if(pinTypes[i] == 'T')
				Device.AddAsset(inputs[i], "toggle " + label, "a toggle button input pin", false, "boolean"); 
			else
				Serial.print("invalid pin type: "); Serial.println(pinTypes[i]);
			wdt_reset();												//make certain that we don't kill the app with the watchdog
		}
		unsigned short relay = 1;
		for(int i = 0 ; i < USEDRELAYSSIZE; i++) {
			Serial.print(relay); Serial.println(" = relay, usedRelays & relays = "); Serial.println(relay & usedRelays);
			if((relay & usedRelays) != 0){
				String label(i);
				Device.AddAsset(relays[i], "relays " + label, "an output relays", true, "boolean"); 
			}
			relay = relay << 1;
			wdt_reset();												//make certain that we don't kill the app with the watchdog
		}
		unsigned int output = 1;
		for(int i = 0 ; i < OUTPUTSSIZE; i++) {
			if((output & usedOutputs) != 0){
				String label(i);
				Device.AddAsset(outputs[i], "output " + label, "an output pin", true, "boolean"); 
			}
			output = output << 1;
			wdt_reset();												//make certain that we don't kill the app with the watchdog
		}
		initState = DEVICECREATED;
		return true;
	}
	return false;
}

//only send if there is a network connection, otherwise skip.
void Send(String value, int id){
	if (initState == SUBSCRIBED && ethClient){
		if(pubSub.connected())
			Device.Send(value, id);
		else 
			initState = 0;					//discovered network failure, reset state
	}
	else{
		Serial.print("no network connection, can't send value '"); Serial.print(value); Serial.print("' to: "); Serial.println(id); 
	}
}

void trySubscribe(){
	if(Device.Subscribe(pubSub))                                   // make certain that we can receive message from the iot platform (activate mqtt)
	{
		initState = SUBSCRIBED;
		for(int i = 0; i < sizeof(relays)/sizeof(relays[0]); i++){
			if(curOutputValues[relays[i]] == true)
				Send("true", relays[i]);
			else
				Send("false", relays[i]);
		}
		wdt_reset();												//make certain that we don't kill the app with the watchdog
		
		for(int i = 0; i < sizeof(outputs)/sizeof(outputs[0]); i++){
			if(curOutputValues[outputs[i]] == true)
				Send("true", outputs[i]);
			else
				Send("false", outputs[i]);
		}
		wdt_reset();												//make certain that we don't kill the app with the watchdog
		for(int i = 0; i < PINTYPESIZE; i++){
			if(pinTypes[i] == 'B' || pinTypes[i] == 'T')
				Send("false", inputs[i]);
		}
		wdt_reset();												//make certain that we don't kill the app with the watchdog
		
		
		Send("Controllino mega - light control", APPID);			//update the application id so that the mobile client can discover the device.
		Send(pinTypes, PINTYPESID);
		Send(String(usedRelays), USEDRELAYSID);
		Send(String(usedOutputs), OUTPUTSID);
		wdt_reset();												//make certain that we don't kill the app with the watchdog
		WatchDog.Ping();												//start the network watchdog
	}
}

void setupNetwork(){
	if(initNetwork() == true){
		initState = ETHSTARTED;
		if(syncDevice())
			trySubscribe();
	}
}

void setupNetworkFast(){
	if(initNetwork() == true){
		initState = DEVICECREATED;
		trySubscribe();
	}
}

void setup()
{    
    Serial.begin(57600);
	for(int i = 0; i < PINTYPESIZE; i++)							//init array to all 0 -> all lights are off when we start up.
		prevPinValues[i] = 0;
    readConfigData();
	wdt_reset();													//if the watchdog rebooted the dev, it is still running, make certain that the app doesn't get in a continous reboot cycle when there is no ethernet connection.
    initPins();
	wdt_reset();													//if the watchdog rebooted the dev, it is still running, make certain that the app doesn't get in a continous reboot cycle when there is no ethernet connection.
	#ifdef CREATEONSTART
	setupNetwork();
	#else
	setupNetworkFast();
	#endif
	wdt_enable(WDTO_4S);
}

unsigned long lastTimeChecked = 0;

void checkNetworkSetup(){
	unsigned long curTime = millis();
	if (curTime >= lastTimeChecked + 20000){								//only check every 20 seconds. If there is a network problem and we check on every run of process, then the system appears to hang or stutter
		lastTimeChecked = curTime;
		if(initState == 0){
			#ifdef CREATEONSTART
			setupNetwork();
			#else
			setupNetworkFast();
			#endif
		}
		else if(initState == ETHSTARTED){
			if(syncDevice()){
				if(Device.Subscribe(pubSub))                                   // make certain that we can receive message from the iot platform (activate mqtt)
					initState = SUBSCRIBED;
			}
		}
		else if(initState == DEVICECREATED && Device.Subscribe(pubSub)){
			initState = SUBSCRIBED;
			WatchDog.Ping();												//start the watchdog
		}
	}
}

                                                                
void loop()
{
	//Serial.println("process start");
	wdt_reset();												//make certain that we don't kill the app with the watchdog
    for(int i = 0; i < PINTYPESIZE; i++){
        if(pinTypes[i] == 'A'){                                 //we can only upload analog values to the fog/cloud for further processing.         
            int value = analogRead(inputs[i]);
            if(value != prevPinValues[i]){
                #ifdef DEBUG 
                Serial.print("value pin "); Serial.print(pinTypes[i]); Serial.print(" = "); Serial.println(value);
                #endif
                prevPinValues[i] = value;
                Send(String(value), inputs[i]);
            }
        }
        else if(pinTypes[i] == 'B'){
            bool value = digitalRead(inputs[i]);
            if(value != (bool)prevPinValues[i]){
                #ifdef DEBUG 
                Serial.print("value pin "); Serial.print(pinTypes[i]); Serial.print(" = "); Serial.println(value);
                #endif
                prevPinValues[i] = (int)value;
                doIOMapping(i, value);
                if(value == 1)
                   Send("true", inputs[i]);
                else
                   Send("false", inputs[i]);
            }
        }
        else if(pinTypes[i] == 'T'){
            bool value = digitalRead(inputs[i]);
            if(value != (bool)prevPinValues[i]){
                if(value == 1){
                    #ifdef DEBUG 
                    Serial.print("value pin "); Serial.print(pinTypes[i]); Serial.print(" = "); Serial.println(value);
                    #endif
                    doToggleIOMapping(i);
                }
                prevPinValues[i] = value;
                if(value == 1)
                   Send("true", inputs[i]);
                else
                  Send("false", inputs[i]);
            }
        }
    }
	if(initState == SUBSCRIBED)		//when we are subscribed, check for network status.
	{
		if(Device.Process() == false)					// if process failed, the connection is lost, so try to recreate later on
			initState = 0;
		else if(WatchDog.CheckPing() == false)
			initState = DEVICECREATED;								//if we lost the connection, reset it the state, so that the connection is recreated.
	}
	checkNetworkSetup();	
}

//check the io map and if there is an output pin defined, send the new value to it.
void doIOMapping(byte ioMapIndex, bool value)
{
    if(ioMap[ioMapIndex] != -1){                //-1 = 255
        #ifdef DEBUG 
        Serial.print("found mapping to activate: "); Serial.println(ioMap[ioMapIndex]);
        #endif
        SetOutputVal(ioMap[ioMapIndex], value);
		if(value == true)
		   Send("true", ioMap[ioMapIndex]);
		else
		  Send("false", ioMap[ioMapIndex]);
    }
}

//check the io map and if there is an output pin defined, send the new value to it.
void doToggleIOMapping(byte ioMapIndex)
{
    if(ioMap[ioMapIndex] != -1){                //-1 = 255
        #ifdef DEBUG 
        Serial.print("found mapping, activate output pin: "); Serial.println(ioMap[ioMapIndex]);
        #endif
		byte index = translatePinToOutputsIndex(ioMap[ioMapIndex]);
		curOutputValues[index] = !curOutputValues[index];
        digitalWrite(ioMap[ioMapIndex], curOutputValues[index]);          //change the actuator status to false
		if (curOutputValues[index])
			Send("true", ioMap[ioMapIndex]);
		else
			Send("false", ioMap[ioMapIndex]);
    }
}


String convertToStr(byte* payload, unsigned int length)
{
    String msgString; 
    char message_buff[length + 1];                        //need to copy over the payload so that we can add a /0 terminator, this can then be wrapped inside a string for easy manipulation
    strncpy(message_buff, (char*)payload, length);        //copy over the data
    message_buff[length] = '\0';                      //make certain that it ends with a null           
          
    msgString = String(message_buff);
    return msgString;
}

//translates the pin nr (output pin or relay) to an index nr of the list of all outputs, which stores the current values.
byte translatePinToOutputsIndex(byte pinNr){
	byte index;
	if(pinNr <= CONTROLLINO_PLUS)
		index = pinNr - 2;
	else if(pinNr <= CONTROLLINO_R15)
		index = pinNr - (2 + 6);
	else 
		index = pinNr - (2 + 6 + 4);
	return index;
}

void SetOutputVal(byte pinNr, bool value){
	digitalWrite(pinNr, value);          //change the actuator status to false
	byte index = translatePinToOutputsIndex(pinNr);
	curOutputValues[index] = value;
}

void resyncDevice(){
	Device.Close();
	initState = ETHSTARTED;				//reset the state so that we can start over correctly again.
	if(syncDevice())
		trySubscribe();
}

// Callback function: handles messages that were sent from the iot platform to this device.
void callback(char* topic, byte* payload, unsigned int length) 
{ 
    int pinNr = Device.GetPinNr(topic, strlen(topic));

    String msgString = convertToStr(payload, length);
    #ifdef DEBUG
    Serial.print("Payload: "); Serial.println(msgString);
    Serial.print("topic: "); Serial.println(topic);
	Serial.print("pin: "); Serial.println(pinNr);
    #endif

	if(!WatchDog.IsWatchDog(pinNr, msgString)){
		if (pinNr == IOMAPID){  
			if(!storeioMap(msgString.c_str())) return;				//if we failed to store the iomap, don't reply back to the server: something went wrong.
		}
		else if(pinNr == PINTYPESID){
			storePinTypes(msgString.c_str());
			resyncDevice();
		}
		else if(pinNr == USEDRELAYSID){
			storeUsedRelays((unsigned short)msgString.toInt());
			resyncDevice();
		}
		else if(pinNr == OUTPUTSID){
			storeOutputs((unsigned int)msgString.toInt());		//toInt() returns a long, so we are save with the unsigned int.
			resyncDevice();
		}
		else{
			msgString.toLowerCase();                  //to make certain that our comparison later on works ok (it could be that a 'True' or 'False' was sent)
			if (msgString == "false")                       //send to an output pin.
				SetOutputVal(pinNr, LOW);
			else if (msgString == "true")
				SetOutputVal(pinNr, HIGH);              //change the actuator status to true
		}
	}
    Send(msgString, pinNr);    
}

