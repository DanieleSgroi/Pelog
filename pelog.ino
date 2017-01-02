/**************************************************************************************************************************************
**
** PELLET DATA LOGGER
**
** (C) 2016 by Daniele Sgroi - daniele.sgroi@gmail.com
**
** VERSION 0.10 - 31 December 2016 - First alpha test
** VERSION 0.90 - 02 January  2017 - First beta upoaded on github
** VERSION 0.91 - 02 January  2017 - Bugfix for IDE 1.8.0
** VERSION 0.92 - 02 January  2017 - Error LED management, improved RTX data managment
**
** License: This code is public domain but you'll buy me a beer if you use this and we meet someday (Beerware license).
**
** Note: interface to the stove is according to following schema:
**
**   ARDUINO GND ------------ GND STOVE (4)
**   ARDUINO TX  ---|<|--*--- RTX STOVE (3) - 1N4148
**                       |
**   ARDUINO RX  --------|
**                            18V STOVE (2)
**                             NC STOVE (1) - first from left seen from back of stove
**
**  1200,8N2 - 1WIRE TTL - 00 xx : READ RAM ADDRESS xx, ANSWER CC vv, CC = vv + xx
** 
** S0 T Fumi              00 5A °C, (Valore)
** S1 Coclea              00 0D (Valore / 40)
** S2 Giri Fumi           00 42 (Valore * 10 + 250) RPM
** S3 T minuti Fase       00 32 (Valore) minuti
** S4 T Aux               00 A7 (Valore) Â°C (T Serbatoio Pellet)
** S5 Flusso              00 BE 
                          00 BF FLUSSO = (BF * 256 + BE) / 100
** S6 Potenza             00 34 (Valore)
** S7 T. Aria input       00 81 (Valore / 2 - 16) °C
** S8 T. Ambiente         00 A6 (Valore / 2) °C
** SA Bar H2O             00 B4 (Valore / 10) Bar
** X1 Temp. H20           00 01 (Valore / 2) °C 
** X2 RTC SEC             00 63 sec, From 00 to 59 
** X3 RTC HOUR            00 65 hrs, From 0 to 23
** X4 RTC MIN             00 66 min, From 0 to 59 
** X5 RTC DOM             00 67 dom, From 1 to 31
** X6 RTC MONTH           00 68 moy, 1 = Gennaio to 12 = December
** X7 RTC YEAR            00 69 yea, Year - 2000 (16 = 20016)
**
**************************************************************************************************************************************/

#define VERSION 0.92

#include <SPI.h>    
#include <SD.h>

//HW PIN definitions
#define PIN_02        2
#define PIN_03        3
#define PIN_04        4
#define PIN_05        5
#define PIN_06        6
#define PIN_07        7
#define PIN_08        8
#define PIN_09        9
#define SD_CS_PIN    10
#define SPI_MOSI_PIN 11
#define SPI_MISO_PIN 12
#define SPI_SCK_PIN  13
#define BUT_0_PIN    A0
#define BUT_1_PIN    A1
#define LED_0_PIN    A2 // cycle timing
#define LED_1_PIN    A3 // any error lit this
#define I2C_BUS_SDA  A4
#define I2C_BUS_SCL  A5
#define ANA_A6       A6
#define ANA_A7       A7

/*************************************************************************
**
** GLOBALS
**
**************************************************************************/

// SD Card file handle
File dataFile;
// stove data
unsigned char ucTempFumi = 0;   // 00 5A Â°C, (Valore)
double dCoclea = 0.0;           // 00 0D (Valore / 40)
unsigned int uiGiriFumi = 0;    // 00 42 (Valore * 10 + 250) RPM
unsigned char ucMinutiFase = 0; // 00 32 (Valore) minuti
unsigned char ucTempAux = 0;    // 00 A7 (Valore) Â°C (T Serbatoio Pellet)
double dFlusso = 0.0;           // 00 BE BF FLUSSO = (BF * 256 + BE) / 100
unsigned char ucPotenza = 0;    // 00 34 (Valore)
double dTempAriaInput = 0.0;    // 00 81 (Valore / 2 - 16) Â°C
double dTempAmbiente = 0.0;     // 00 A6 (Valore / 2) Â°C
double dBarH2O = 0.0;           // 00 B4 (Valore / 10) Bar
double dTempH20 = 0.0;          // 00 01 (Valore / 2) Â°C 
unsigned char ucRtcSec = 0;     // 00 63 sec, From 00 to 59 
unsigned char ucRtcHour = 0;    // 00 65 hrs, From 0 to 23
unsigned char ucRtcMin = 0;     // 00 66 min, From 0 to 59 
unsigned char ucRtcDay = 0;     // 00 67 day, From 1 to 31
unsigned char ucRtcMonth = 0;   // 00 68 moy, 1 = Gennaio to 12 = December
unsigned int uiRtcYear = 0;     // 00 69 yea, Year - 2000 (16 = 20016)
// Serial RTX
unsigned char ucBufferRtx[] = {0, 0, 0, 0};
unsigned char ucLoopIdx = 0;
String sDataString;
File fDataFile;

/*************************************************************************
**
** FUNCTIONS
**
**************************************************************************/

bool bReadStoveRam(unsigned char Addr) {

  bool bDataValid = false;
 
  memset(ucBufferRtx, 0x00, 4);

  Serial.flush();
    
  Serial.write(0x00); // read RAM address command. 0x20 = read EEPROM address command.
  Serial.write(Addr);

  delay(250); // let the stove understand and answer
      
  if (Serial.readBytes(ucBufferRtx, 4) == 4) {
    if(((ucBufferRtx[1] + ucBufferRtx[3]) % 256) == ucBufferRtx[2]) {
      bDataValid = true;    
    }
    else {
      bDataValid = false;    
    };
  }
  else {
    bDataValid = false;    
  };

  if (!bDataValid) 
    digitalWrite(LED_1_PIN, HIGH);
  else
    digitalWrite(LED_1_PIN, LOW);
    
  return bDataValid;
    
}

/*************************************************************************
**
** SETUP
**
**************************************************************************/

void setup(void) {

  Serial.begin(1200, SERIAL_8N2); // for stove interface

  //Serial.print("Pellet Data Logger Version: ");
  //Serial.print(VERSION, 1);
  //Serial.println(" - (C)2016 Daniele Sgroi");

  pinMode(SD_CS_PIN,   OUTPUT);
  pinMode(SPI_SCK_PIN, OUTPUT);
  pinMode(BUT_0_PIN,   INPUT_PULLUP);
  pinMode(BUT_1_PIN,   INPUT_PULLUP);
  pinMode(LED_0_PIN,   OUTPUT);
  pinMode(LED_1_PIN,   OUTPUT);
  
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(LED_0_PIN, LOW);
  digitalWrite(LED_1_PIN, LOW);
  
  //Serial.println("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card failed, or not present. Stopped.");
    digitalWrite(LED_1_PIN, HIGH);
    // don't do anything more
    while(1);
  }

  delay(250);
  
}

/*************************************************************************
**
** MAIN ENDLESS LOOP
**
**************************************************************************/

void loop(void) {

  // scheduler
  switch (ucLoopIdx++) {
    
    case 0:
      digitalWrite(LED_0_PIN, LOW);
      if(bReadStoveRam(0x5A))
        ucTempFumi = ucBufferRtx[3];            // 00 5A °C, (Valore)
    break;

    case 1:
      if(bReadStoveRam(0x0D))
        dCoclea = ucBufferRtx[3] / 40.0F;       // 00 0D (Valore / 40)
      break;

    case 2:
      if(bReadStoveRam(0x42))
        uiGiriFumi = 250 +  ucBufferRtx[3] * 10; // 00 42 (Valore * 10 + 250) RPM
      break;

    case 3:
      if(bReadStoveRam(0x32))
        ucMinutiFase = ucBufferRtx[3];          // 00 32 (Valore) minuti
      break;
      
    case 4:
      if(bReadStoveRam(0xA7))
        ucTempAux = ucBufferRtx[3];             // 00 A7 (Valore) °C (T Serbatoio Pellet)
      break;
      
    case 5:
      if(bReadStoveRam(0xBE))
        dFlusso = ucBufferRtx[3]; 
      if(bReadStoveRam(0xBF)) {
        dFlusso += ucBufferRtx[3] * 256;
        dFlusso = dFlusso / 100.0F;
      }
      //dFlusso = (1.0F * ucReadStoveRam(0xBE) + 256.0F * ucReadStoveRam(0xBF)) / 100.0F; // 00 BE BF FLUSSO = (BF * 256 + BE) / 100
      break;
      
    case 6:
      if(bReadStoveRam(0x34))
        ucPotenza = ucBufferRtx[3];            // 00 34 (Valore)
      break;
      
    case 7:
      if(bReadStoveRam(0x81))
        dTempAriaInput = (ucBufferRtx[3] / 2.0F) - 16 ;    // 00 81 (Valore / 2 - 16) °C
      break;
      
    case 8:
      if(bReadStoveRam(0xA6))
        dTempAmbiente = ucBufferRtx[3] / 2.0F; // 00 A6 (Valore / 2) °C
      break;
      
    case 9:
      if(bReadStoveRam(0xB4))
        dBarH2O = ucBufferRtx[3] / 10.0F;     // 00 B4 (Valore / 10) Bar
      break;
      
    case 10:
      if(bReadStoveRam(0x01))
        dTempH20 = ucBufferRtx[3] / 2.0F;     // 00 01 (Valore / 2) °C 
      break;
      
    case 11:
      if(bReadStoveRam(0x63))
        ucRtcSec = ucBufferRtx[3];     // 00 63 sec, From 00 to 59 
      break;
      
    case 12:
      if(bReadStoveRam(0x65))
        ucRtcHour = ucBufferRtx[3];    // 00 65 hrs, From 0 to 23
      break;
      
    case 13:
      if(bReadStoveRam(0x66))
        ucRtcMin = ucBufferRtx[3];     // 00 66 min, From 0 to 59 
      break;
      
    case 14:
      if(bReadStoveRam(0x67))
        ucRtcDay = ucBufferRtx[3];     // 00 67 day, From 1 to 31
      break;
      
    case 15:
      if(bReadStoveRam(0x68))
        ucRtcMonth = ucBufferRtx[3];   // 00 68 moy, 1 = Gennaio to 12 = December
      break;
      
    case 16:
      if(bReadStoveRam(0x69))
        uiRtcYear = 2000 + ucBufferRtx[3];    // 00 69 yea, Year - 2000 (16 = 20016)
      break;
           
    case 17:
      // make a string for assembling the data to log:
      sDataString = String(uiRtcYear);          //  1 column
      sDataString += ":";
      sDataString += String(ucRtcMonth);        //  2
      sDataString += ":";
      sDataString += String(ucRtcDay);          //  3
      sDataString += ":";
      sDataString += String(ucRtcHour);         //  4
      sDataString += ":";
      sDataString += String(ucRtcMin);          //  5
      sDataString += ":";
      sDataString += String(ucRtcSec);          //  6
      sDataString += ":";
      sDataString += String(ucTempFumi);        //  7
      sDataString += ":";
      sDataString += String(dCoclea, 3);        //  8
      sDataString += ":";
      sDataString += String(uiGiriFumi);        //  9
      sDataString += ":";
      sDataString += String(ucMinutiFase);      // 10
      sDataString += ":";
      sDataString += String(ucTempAux);         // 11
      sDataString += ":";
      sDataString += String(dFlusso, 1);        // 12
      sDataString += ":";
      sDataString += String(ucPotenza);         // 13
      sDataString += ":";
      sDataString += String(dTempAriaInput, 1); // 14
      sDataString += ":";
      sDataString += String(dTempAmbiente, 1);  // 15
      sDataString += ":"; 
      sDataString += String(dBarH2O, 1);        // 16
      sDataString += ":";
      sDataString += String(dTempH20, 1);       // 17
      break;

    case 18:
      // open the file. only one file can be open at a time
      fDataFile = SD.open("datalog.txt", FILE_WRITE); 

      if (fDataFile) {
        digitalWrite(LED_1_PIN, LOW);
        // if the file is available, write to it:
        fDataFile.println(sDataString);
        fDataFile.close();
      }
      else {
        // if the file isn't open, pop up an error:
        Serial.println("error opening datalog.txt. Stopped.");
        digitalWrite(LED_1_PIN, HIGH);
      }
      break;

    default:
      digitalWrite(LED_0_PIN, HIGH);
      ucLoopIdx = 0; // reset scheduler step
      
  } // end switch

  delay(25); // tune overall loop duration

}

/*************************************************************************
**
** END OF FILE
**
**************************************************************************/
