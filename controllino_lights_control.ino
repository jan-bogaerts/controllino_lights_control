#include <Ethernet.h>
#include <EthernetClient.h>

#include <PubSubClient.h>
#include <ATT_IOT.h>                            //AllThingsTalk IoT library
#include <SPI.h>                                //required to have support for signed/unsigned long type..
#include <EEPROM.h>                             //get/store configs
#include <Controllino.h>

#define DEBUG                                   //turn off to remove serial logging so it runs faster and takes up less mem.


// Enter below your client credentials. 
//These credentials can be found in the configuration pane under your device in the smartliving.io website 

#define deviceId "" // Your device id comes here
#define clientId "" // Your client id comes here;
#define clientKey "" // Your client key comes here;

//IPAddress localMqttServer(192, 168, 1, 121);

ATTDevice Device(deviceId, clientId, clientKey);            //create the object that provides the connection to the cloud to manager the device.
char httpServer[] = "api.smartliving.io";                   // HTTP API Server host                  
char mqttServer[] = "broker.smartliving.io";            // MQTT Server Address 


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
//#define ERRORID 95
        
//maps input pins with output pins      
//byte ioMap[IOMAPSIZE] = {22, 23, 24, 25, 26, 27, 28, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
byte ioMap[IOMAPSIZE] = {0x25, 23, 24, 25, 26, 27, 28, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char pinTypes[PINTYPESIZE + 1] = "TTTTTTTNNNNNNNNNNNNNN";               //specify for each input pin if it is analog, digital button or digital toggle, or not used
int prevPinValues[PINTYPESIZE];                                     //used to keep track of the previous state of the input pins, for analog, it stores the prev value, for digital, it  stores the prev value of the pin and optionally (for toggle) the prev state of the output.
unsigned short usedRelays = 0x807F;                                                   //bit mask to specify which relays are used or not.
unsigned int usedOutputs = 0;                                                        //bit mask to speivy which digitial outputs are used -> so we know how to activate them.

//required for the device
void callback(char* topic, byte* payload, unsigned int length);
EthernetClient ethClient;
PubSubClient pubSub(mqttServer, 1883, callback, ethClient);


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
        #ifdef DEBUG 
        Serial.print("used digital outputs: "); Serial.println(usedOutputs, HEX);
        #endif
    }
}

//stores the new definition of active outputs into global mem and flash (if changed)
void storeOutputs(int newValue)
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
        EEPROM.write(0, 1);                                     //indicate that the eeprom has memory stored
    }
}

//stores the new definition of used relays into global mem and flash (if changed)
void storeUsedRelays(short newValue)
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
void storePinTypes(char *newValues)
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


//checks if the 2 arrays are different, if so, the new array is stored into the new one and into the eeprom
void storeioMap(byte *newValues)
{
    #ifdef DEBUG 
    Serial.println("storing ioMap");
    #endif
    bool different = false;
    for(int i = 0; i < IOMAPSIZE; i++){
        if(newValues[i] != ioMap[i]){
            different = true;
            ioMap[i] = newValues[i];
            #ifdef DEBUG 
            Serial.print(i); Serial.print(" = "); Serial.println(newValues[i]);
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
void initNetwork()
{
    byte mac[] = {  0x90, 0xA2, 0xDA, 0x0D, 0x8D, 0x3D };       // Adapt to your Arduino MAC Address 
    if (Ethernet.begin(mac) == 0)                             // Initialize the Ethernet connection:
    { 
        #ifdef DEBUG 
        Serial.println(F("DHCP failed,end"));
        #endif
        while(true);                                            //we failed to connect, halt execution here. 
    }
    delay(1000);                                            //give the Ethernet shield a second to initialize:
}

//prepare the device in the fog/cloud -> declare all the assets and the correct types
void syncDevice()
{
    Device.Connect(&ethClient, httpServer);
    Device.AddAsset(IOMAPID, "IO map", "link inputs with outputs.", true, "{\"type\": \"array\", \"items\":{\"type\":\"integer\"}}");   // Create the Digital Actuator asset for your device
    Device.AddAsset(PINTYPESID, "pin types", "specify for each input pin if it is analog (A), button (B), toggle (T) or not used (any other).", true, "string"); 
    Device.AddAsset(USEDRELAYSID, "used relays", "Specify which relays (outputs) are used, as a bitfield (16 bits)", true, "integer"); 
    Device.AddAsset(OUTPUTSID, "used outputs", "Specify which digital outputs are used (20 bits)", true, "integer"); 
    //Device.AddAsset(ERRORID, "last error", "the last error produced by the device (after bootup)", false, "string"); 
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
    }
    unsigned short relay = 1;
    for(int i = 0 ; i < USEDRELAYSSIZE; i++) {
        Serial.print(relay); Serial.println(" = relay, usedRelays & relays = "); Serial.println(relay & usedRelays);
        if((relay & usedRelays) != 0){
            String label(i);
            Device.AddAsset(relays[i], "relays " + label, "an output relays", true, "boolean"); 
        }
        relay = relay << 1;
    }
    
    unsigned int output = 1;
    for(int i = 0 ; i < OUTPUTSSIZE; i++) {
        if((output & usedOutputs) != 0){
            String label(i);
            Device.AddAsset(outputs[i], "output " + label, "an output pin", true, "boolean"); 
        }
        output = output << 1;
    }
}

void setup()
{    
    Serial.begin(9600);
    readConfigData();
    initPins();
    
    initNetwork();
    syncDevice();
    Device.Subscribe(pubSub);                                   // make certain that we can receive message from the iot platform (activate mqtt)
}
                                                                
void loop()
{
    for(int i = 0; i < PINTYPESIZE; i++){
        if(pinTypes[i] == 'A'){                                 //we can only upload analog values to the fog/cloud for further processing.         
            int value = analogRead(inputs[i]);
            if(value != prevPinValues[i]){
                #ifdef DEBUG 
                Serial.print("value pin "); Serial.print(pinTypes[i]); Serial.print(" = "); Serial.println(value);
                #endif
                prevPinValues[i] = value;
                Device.Send(String(value), inputs[i]);
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
                Device.Send(String(value), inputs[i]);
            }
        }
        else if(pinTypes[i] == 'T'){
            bool value = digitalRead(inputs[i]);
            bool* prevValues = (bool*)&(prevPinValues[i]);
            if(value != prevValues[0]){
                if(value == 1){
                    #ifdef DEBUG 
                    Serial.print("value pin "); Serial.print(pinTypes[i]); Serial.print(" = "); Serial.println(value);
                    #endif
                    prevValues[1] = !prevValues[1];
                    doIOMapping(i, prevValues[1]);
                    Device.Send(String(prevValues[1]), inputs[i]);
                }
                prevValues[0] = value;
            }
        }
    }
    Device.Process(); 
}

//check the io map and if there is an output pin defined, send the new value to it.
void doIOMapping(byte ioMapIndex, bool value)
{
    if(ioMap[ioMapIndex] != -1){                //-1 = 255
        #ifdef DEBUG 
        Serial.print("found mapping to activate: "); Serial.println(ioMap[ioMapIndex]);
        #endif
        digitalWrite(ioMap[ioMapIndex], value);
        Device.Send(String(value), ioMap[ioMapIndex]);
    }
}

//returns the pin nr found in the topic
int GetPinNr(char* topic, int topicLength)
{
    int result = topic[topicLength - 9] - 48;
    #ifdef DEBUG 
    Serial.print("len: "); Serial.println(topicLength - 10 - sizeof(deviceId));
    Serial.print("content: "); Serial.println(topic[topicLength - 10 - sizeof(deviceId)]);
    #endif
    if(topic[topicLength - 10 - sizeof(deviceId)] != '/'){
        result += (topic[topicLength - 10 - sizeof(deviceId)] - 48) * 10;
    }   
    return result;
}

String convertToStr(byte* payload, unsigned int length)
{
    String msgString; 
    char message_buff[length + 1];                        //need to copy over the payload so that we can add a /0 terminator, this can then be wrapped inside a string for easy manipulation
    strncpy(message_buff, (char*)payload, length);        //copy over the data
    message_buff[length] = '\0';                      //make certain that it ends with a null           
          
    msgString = String(message_buff);
    msgString.toLowerCase();                  //to make certain that our comparison later on works ok (it could be that a 'True' or 'False' was sent)
    return msgString;
}

// Callback function: handles messages that were sent from the iot platform to this device.
void callback(char* topic, byte* payload, unsigned int length) 
{ 
    int pinNr = GetPinNr(topic, strlen(topic));

    String msgString = convertToStr(payload, length);
    #ifdef DEBUG
    Serial.print("Payload: "); Serial.println(msgString);
    Serial.print("topic: "); Serial.println(topic);
    #endif

    if (pinNr == IOMAPID)  
        storeioMap(payload);
    else if(pinNr == PINTYPESID){
        storePinTypes((char*)payload);
        syncDevice();
    }
    else if(pinNr == USEDRELAYSID){
        //String msgString = convertToStr(payload, length);
        storeUsedRelays((short)msgString.toInt());
        syncDevice();
    }
    else if(pinNr == OUTPUTSID){
        //String msgString = convertToStr(payload);
        storeOutputs((int)msgString.toInt());
        syncDevice();
    }
    else{
        //String msgString = convertToStr(payload);
        if (msgString == "false")                       //send to an output pin.
            digitalWrite(pinNr, LOW);          //change the actuator status to false
        else if (msgString == "true")
            digitalWrite(pinNr, HIGH);              //change the actuator status to true
    }
    Device.Send(msgString, pinNr);    
}

