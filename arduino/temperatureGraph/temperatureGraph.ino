// Needed for Ethernet Web Server
#include <SPI.h>
#include <Ethernet.h>

// Needed to communicate with Dallas 18B20 One Wire temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>

// Needed to communicate with 4x20 I2C LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/******************************************************************************/
/* Ethernet Globals
/******************************************************************************/
// Enter a MAC address (choose one, or locate Ethernet shield MAC sticker)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Initialize Ethernet server with default HTTP port 80
EthernetServer server(80);
/******************************************************************************/

/******************************************************************************/
/* OneWire / DallasTemperature Globals
/******************************************************************************/
// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS 2

// Create instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

// Variable to cache the address of our single temperature sensor
DeviceAddress deviceAddress;
/******************************************************************************/

/******************************************************************************/
/* LCD Globals
/******************************************************************************/
// Set the I2C LCD address to 0x3f for this 20 chars 4 line display
// Set the pins on the I2C chip used for LCD connections:
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x3f, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Used to know how many characters need to be blanked out
#define LINES 4
#define CHARS_PER_LINE 20

// Set which line/char contains what information
#define IP_LINE 3
#define TEMP_LINE 1
#define TEMP_CHAR 7

// Variable to hold location to print out dynamic IP
int ipValueChar;
/******************************************************************************/

/******************************************************************************/
/* Macro Functions
/******************************************************************************/
// Helper macro to print dynamic output and blank the rest of the line
#define LCD_BLANK_LINE_AFTER_OUTPUT(charStart, line, printStatement)           \
{                                                                              \
  lcd.setCursor((charStart), (line));                                          \
  int charsWritten = (printStatement);                                         \
  for (int i = (charStart) + charsWritten; i < CHARS_PER_LINE; i++) {          \
    lcd.print(" ");                                                            \
  }                                                                            \
}
/******************************************************************************/

// outputs current IP address in correct spot
void updateLcdIp(void) {
  LCD_BLANK_LINE_AFTER_OUTPUT(ipValueChar,
                              IP_LINE,
                              lcd.print(Ethernet.localIP()));
}

// outputs Fahrenheight temp
// (mostly centered in 20 char display for 10 < temp < 100)
void updateLcdTemperature(void) {                              
  LCD_BLANK_LINE_AFTER_OUTPUT(TEMP_CHAR,
                              TEMP_LINE,
                              lcd.print(sensors.getTempF(deviceAddress), 1) + 
                              lcd.print((char)223) + 
                              lcd.print("F"));
}

void setup(void)
{
  pinMode(3, OUTPUT); // We'll use pin 3 as a GND for temp sensor
  digitalWrite(3, LOW); // With pin 2 as data, this keeps 2, 3, 4 close
  
  pinMode(4, OUTPUT); // Pin 4 high disables micro-SD on Ethernet shield
  digitalWrite(4, HIGH); // We'll use pin 4 as a VDD source for temp sensor
  
  // Start temp sensor library; get the address of our sensor; request reading
  // This is done early so that first loop temp reading is accurate
  sensors.begin();
  sensors.getAddress(deviceAddress, 0);
  sensors.requestTemperaturesByAddress(deviceAddress);
  
  // Start LCD
  lcd.begin(CHARS_PER_LINE, LINES);
  lcd.backlight(); // turn on backlight

  // Start the Ethernet connection
  if (Ethernet.begin(mac) == 0) {
    lcd.setCursor(0, 0); 

    lcd.print("DHCP Failed");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  server.begin();
  
  // Initial LCD printouts for unchanging output
  lcd.setCursor(0, IP_LINE);
  ipValueChar = lcd.print("IP: ");
  
  // Print out (potentially dynamic) IP address
  updateLcdIp();
}

void printHttpResponse(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
  
  client.print("{ \"temperatureF\" : ");
  client.print(sensors.getTempF(deviceAddress), 1);      
  client.println(" }"); 
}

void loop(void)
{
  // always request a new temperature reading
  sensors.requestTemperaturesByAddress(deviceAddress);
  
  // always update LCD with new temperature reading
  updateLcdTemperature(); 

  // check for incoming clients
  if (EthernetClient client = server.available()) {
    boolean currentLineIsBlank = true;

    while (client.connected() && client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (!currentLineIsBlank) { // starting a new line
          currentLineIsBlank = true;
        } else { // http requests end with a blank line
          printHttpResponse(client);
          break;
        }
      } else if (c != '\r') {
        // there's an actual character on the current line
        currentLineIsBlank = false;
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}
