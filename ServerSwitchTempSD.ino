#include <OneWire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <DallasTemperature.h>


#define ONE_WIRE_BUS 9
#define TEMPERATURE_PRECISION 9
// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   60

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0x2E, 0x3D, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 33); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80
File webFile;               // the web page file on the SD card
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer
boolean RELE_state[2] = {0}; // stores the states of the LEDs
float temp1, temp2;
unsigned long lastUpdate = 0;
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress insideThermometer, outsideThermometer;


void setup()
{
    // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    
    Serial.begin(9600);       // for debugging
    
    // initialize SD card
    Serial.println("Initializing SD card...");
    if (!SD.begin(4)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;    // init failed
    }
    Serial.println("SUCCESS - SD card initialized.");
    // check for index.htm file
    if (!SD.exists("index.htm")) {
        Serial.println("ERROR - Can't find index.htm file!");
        return;  // can't find index file
    }
    Serial.println("SUCCESS - Found index.htm file.");
   
   
    pinMode(5, OUTPUT);  //RELE_2
    pinMode(6, OUTPUT);  //RELE_1
    
    
    Ethernet.begin(mac, ip);  // initialize Ethernet device
    server.begin();           // start to listen for clients
    
   sensors.begin();

  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1"); 

  // set the resolution to 9 bit
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);
   
}

void loop() {
    updateTemperature();
    EthernetClient client = server.available();  // try to get client

    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   
                char c = client.read(); // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;         
                    req_index++;
                } 
                if (c == '\n' && currentLineIsBlank) {
                    client.println("HTTP/1.1 200 OK"); 
                    if (StrContains(HTTP_req, "ajax_inputs")) {
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();
                        SetLEDs(); 
                        XML_response(client);
                    }
                    else {  
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        webFile = SD.open("index.htm");   
                        if (webFile) {
                            while(webFile.available()) {
                                client.write(webFile.read()); 
                            }
                            webFile.close();
                        }
                    }
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                if (c == '\n') {
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); 
    } // end if (client)
}

void updateTemperature(){
  unsigned long time = millis();
  if((time - lastUpdate) > 10000 || lastUpdate == -100.0){
    lastUpdate = time;
    sensors.requestTemperatures();
    temp1 = sensors.getTempC(insideThermometer);
    temp2 = sensors.getTempC(outsideThermometer);
  } else {
    return;
  }
}

//converter stroki
String floatToString(float value, byte precision){
  int intVal = int(value);
  unsigned int frac;
  if(intVal >= 0){
    frac = (value - intVal) * precision;
  } 
  else {
    frac = (intVal - value) * precision;
  }
  return String(intVal) + "." + String(frac);
}
 

// checks if received HTTP request is switching on/off LEDs
// also saves the state of the LEDs
void SetLEDs(void)
{
    // RELE 1 (pin 6)
    if (StrContains(HTTP_req, "RELE=1")) {
        RELE_state[0] = 1; 
        digitalWrite(6, HIGH);
    }
    else if (StrContains(HTTP_req, "RELE=0")) {
        RELE_state[0] = 0;  
        digitalWrite(6, LOW);
    }
    // RELE 2 (pin 5)
    if (StrContains(HTTP_req, "RELE2=1")) {
        RELE_state[0] = 1;  
        digitalWrite(5, HIGH);
    }
    else if (StrContains(HTTP_req, "RELE2=0")) {
        RELE_state[0] = 0;
        digitalWrite(5, LOW);
    }
}

void XML_response(EthernetClient cl)
{
    String temp_val1, temp_val2;             
    
    if(temp1 != -100.0){
      temp_val1 = floatToString(temp1, 100);
    }
    if(temp2 != -100.0){
      temp_val2 = floatToString(temp2, 100);
    }
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    
        cl.print("<analog>");
        cl.print(temp_val1);
        cl.println("</analog>");
        cl.print("<analog>");
        cl.print(temp_val2);
        cl.println("</analog>");
    // RELE1
    cl.print("<RELE1>");
    if (RELE_state[0]) {
        cl.print("on");
    }
    else {
        cl.print("off");
    }
    cl.println("</RELE1>");
    
    // RELE2
    cl.print("<RELE2>");
    if (RELE_state[0]) {
        cl.print("on");
    }
    else {
        cl.print("off");
    }
    cl.println("</RELE2>");
    
    cl.print("</inputs>");
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}
