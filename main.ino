//----------------------------------------------------------------------------------------------
//---Software M5 IoT (GS1 / GS2)----------------------------------------------------------------
//----------------------------------------------------------------------------------------------


#include <M5Stack.h>
#include <ezTime.h>
#include <WiFi.h>
#include <M5ez.h>
#include <CRC.h>
#include "ThingSpeak.h"
//#include <SimpleTimer.h>

#define   RXD2          16                                                                  
#define   TXD2          17                                                                  
#define   RW_RS485      2                                                                  

#define   hv_u_corr     47.3933 
#define   hv_i_corr     2.1505
#define   t_gas_corr    0.25
#define   t_box_corr    0.0625

#define   screen_update 299;

WiFiClient client;

char ssid[] = "oekosolve_gast2";                      // Integration Plons 1
char pass[] = "oekosolve2013";                        // Integration Plons

//unsigned long myChannelNumber = 1002507;            //OekoSolve
//const char *  myWriteAPIKey   = "SVDCJBIUZA3ZVFZP";

unsigned long   myChannelNumber = 1023328;            //Homeoffice
const char *    myWriteAPIKey   = "RI7PP5KP9DYU9UO9";

uint8_t         inbuffer[128];
int             inbuffer_size;
uint8_t         arr[6]          = {0x1F, 0x03, 0x00, 0x00, 0x00, 0x14};                         
uint16_t        c16             = CRC::crc16(arr, sizeof(arr));                                 

volatile int             vers_m          = 0;
volatile int             vers_s          = 0;
volatile int             state           = 0;
volatile float           volt            = 0;
volatile float           uamp            = 0;
volatile float           leistung        = 0;
volatile float           t_gas           = 0;
volatile float           t_box           = 0;

bool            con             = false;
int             queuesize       = 0;
int             counter         = 0;
int             loopcnt         = 0;
int             delay_rx        = 0;
int             holding_reg[64];

volatile int    y_pos           = 0;

//----------------------------------------------------------------------------------------------
int readRegister(char slave_addr, int start_addr, int reg_number){
//----------------------------------------------------------------------------------------------
  arr[0] = slave_addr;                                // Slave ID
  arr[1] = 0x03;                                      // Function
  arr[2] = highByte(start_addr);                      // Start Adress
  arr[3] = lowByte(start_addr);
  arr[4] = highByte(reg_number);                      // Number of Registers
  arr[5] = lowByte(reg_number);
  c16    = CRC::crc16(arr, sizeof(arr));
  
  digitalWrite(RW_RS485, HIGH);                       // --- Read
  Serial2.write(arr, 6);
  Serial2.write(lowByte(c16));
  Serial2.write(highByte(c16));
//  for(int y = queuesize ; queuesize == Serial2.availableForWrite() ; queuesize=queuesize){
//    
//  }
  while(queuesize != Serial2.availableForWrite()){
    
  }
  delay(2);
  digitalWrite(RW_RS485, LOW);                        // --- End Read
  Serial2.flush();

  delay_rx = 25+2*reg_number;

  delay(delay_rx);                                    // Warten bis alle Zeichen eingegangen sind
  
  //--- Read Serial Buffer
  inbuffer_size = Serial2.available();
  Serial2.readBytes(inbuffer, inbuffer_size);


  if(inbuffer_size==(2*reg_number+5)){                // Anzahl = Valid
    int k = 0;
    for(int i=0;i<reg_number;i++){
      holding_reg[i] = inbuffer[k+3]*256+inbuffer[k+4];  // Umrechnen auf 16Bit Werte
      k = k+2;      
    }
    return 1;
  }
  else{                                               // Erwartetet Menge stimmt nicht => Verwerfen
    return 0;
  }
}

//----------------------------------------------------------------------------------------------
uint16_t writeThingSpeak(){                                         //Log Data on ThingSpeak.com
//----------------------------------------------------------------------------------------------
        ThingSpeak.setField(1,state); 
        ThingSpeak.setField(2,volt);
        ThingSpeak.setField(3,uamp);
          leistung = ((volt * uamp)/1000000);
        ThingSpeak.setField(4,leistung);
        ThingSpeak.setField(5,t_gas);
        ThingSpeak.setField(6,t_box);
        ThingSpeak.setField(8,vers_s);    
        ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        
        return 20000;                                 //repeat after time [ms]
}

//----------------------------------------------------------------------------------------------
uint16_t getAllData(){                                 //get U,I,t_gas,t_box,vers_m,vers_s,state
//----------------------------------------------------------------------------------------------

  if(readRegister(31,0,21)){                          //Read 0-20

    state = holding_reg[0];                           //get state

    volt = holding_reg[1];                            //get voltage
    volt = volt*hv_u_corr;

    uamp = holding_reg[2];                            //get current
    uamp = uamp*hv_i_corr;           

    t_gas = holding_reg[8];                           //get t_gas
    t_gas = t_gas/4;

    t_box = holding_reg[11];                          //get t_box
    t_box = t_box/16; 

    vers_s = holding_reg[19];                         //get version slave
  }
    
        return 1999;                                  //repeat after time [ms]
}

//----------------------------------------------------------------------------------------------
uint16_t refreshWerteScreen(){                                  //angezeigte Werte Aktualisieren
//----------------------------------------------------------------------------------------------

      M5.Lcd.setTextColor(TFT_WHITE);

      M5.Lcd.fillRoundRect(210, 36, 99, 27, 8, ez.screen.background());
      
        M5.Lcd.setCursor(215, y_pos+57);              //Voltage
        M5.Lcd.printf("%.0f", volt);      

      M5.Lcd.fillRoundRect(210, 65, 99, 27, 8, ez.screen.background());  
        
        M5.Lcd.setCursor(215, y_pos+86);              //Current
        M5.Lcd.printf("%.0f", uamp);      

      M5.Lcd.fillRoundRect(210, 94, 99, 27, 8, ez.screen.background());  
        
        M5.Lcd.setCursor(215, y_pos+115);             //Power
        leistung = volt*uamp;
        M5.Lcd.printf("%.0f", leistung);

return screen_update;                                 //repeat after time [ms]
}

//----------------------------------------------------------------------------------------------
uint16_t refreshTempScreen(){                                                  //get t_gas,t_box
//----------------------------------------------------------------------------------------------

      M5.Lcd.setTextColor(TFT_WHITE);

      M5.Lcd.fillRoundRect(210, 36, 99, 27, 8, ez.screen.background());
      
        M5.Lcd.setCursor(215, y_pos+57);              //Abgas-Temp.
        M5.Lcd.printf("%.0f", t_gas);      

      M5.Lcd.fillRoundRect(210, 65, 99, 27, 8, ez.screen.background());  
        
        M5.Lcd.setCursor(215, y_pos+86);              //Box-Temp.
        M5.Lcd.printf("%.0f", t_box);      
      
return screen_update;                                 //repeat after time [ms]
}

//----------------------------------------------------------------------------------------------
uint16_t refreshSystemScreen(){                                        //get vers_m,vers_s,state
//----------------------------------------------------------------------------------------------

      M5.Lcd.setTextColor(TFT_WHITE);

      M5.Lcd.fillRoundRect(210, 36, 99, 27, 8, ez.screen.background());
      
        M5.Lcd.setCursor(215, y_pos+57);              //Version Master
        if(vers_m > 0){
          M5.Lcd.printf("%.0d", vers_s);     
        }
        else{
          M5.Lcd.printf("0");                         //If no Val print "0"
        }

      M5.Lcd.fillRoundRect(210, 65, 99, 27, 8, ez.screen.background());  
        
        M5.Lcd.setCursor(215, y_pos+86);              //Version Slave
        if(vers_s > 0){
          M5.Lcd.printf("%.0d", state);     
        }
        else{
          M5.Lcd.printf("0");                         //If no Val print "0"
        }                

return screen_update;                                 //repeat after time [ms]
}

void setup() {


  #include <themes/oeko.h>
  #include <themes/default.h>

  M5.begin();

    M5.Power.begin();                                 // No Power-Off @ 24V
    M5.Power.setLowPowerShutdown(false);
    M5.Power.setPowerBoostKeepOn(true);
    M5.Power.setPowerVin(true);
    M5.Power.setAutoBootOnLoad(true);
    M5.Power.setPowerVin(true);

  ez.addEvent(writeThingSpeak);
  ez.addEvent(getAllData);
  ez.addEvent(refreshWerteScreen);
  ez.addEvent(refreshTempScreen);
  ez.addEvent(refreshSystemScreen);
  
  ez.begin();
 
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);        // Init for RS485
    queuesize = Serial2.availableForWrite();
    pinMode(RW_RS485, OUTPUT);      
    digitalWrite(RW_RS485, LOW); 

  WiFi.setHostname("OS-Gateway");
  WiFi.begin(ssid, pass);
//    while(WiFi.status() != WL_CONNECTED) {
//
//    }
  
  ThingSpeak.begin(client);
}


void loop() {

//----------------------------------------------------------------------------------------------
//---Home---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------- 

  ez.removeEvent(refreshWerteScreen);
  ez.removeEvent(refreshTempScreen);
  ez.removeEvent(refreshSystemScreen);  
   
  ezMenu mainmenu("OekoSolve");
  
  mainmenu.txtBig();
  
  mainmenu.addItem("Werte", mainmenu_werte);
  mainmenu.addItem("Temp.", mainmenu_temp);
  mainmenu.addItem("System", mainmenu_system);

  mainmenu.addItem("");
  mainmenu.addItem("Einstellungen", ez.settings.menu);
  
  mainmenu.upOnFirst("last|up");
  mainmenu.downOnLast("first|down");

  mainmenu.runOnce();
}

//----------------------------------------------------------------------------------------------
//---Werte--------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
void mainmenu_werte() {

  ez.removeEvent(refreshTempScreen);
  ez.removeEvent(refreshSystemScreen); 

  ez.addEvent(refreshWerteScreen);
                               
  ezMenu submenu("Werte");

  submenu.txtBig();

  submenu.addItem("Spannung[kV]");                    //Spannung[kV]
  
  submenu.addItem("Strom[uA]");                       //Strom[uA]
  
  submenu.addItem("Leistung[P]");                     //Leistung[P]

  submenu.addItem("");
  submenu.addItem("Exit");                            //Zurück zum Hauptmenü/Hom

  submenu.run();
}

//----------------------------------------------------------------------------------------------
//---Temperaturen-------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
void mainmenu_temp() {

  ez.removeEvent(refreshWerteScreen);
  ez.removeEvent(refreshSystemScreen); 

  ez.addEvent(refreshTempScreen);
   
  ezMenu submenu("Temperaturen");
  
  submenu.txtBig();

  submenu.addItem("Abgas-Temp.[°C]");                 //Abgas-Temperatur[°C]
    
  submenu.addItem("Box-Temp.[°C]");                   //Box-Temperatur[°C]

  submenu.addItem("");
  submenu.addItem("Exit");

  submenu.run();
}

//----------------------------------------------------------------------------------------------
//---System-------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
void mainmenu_system() { 

  ez.removeEvent(refreshWerteScreen);
  ez.removeEvent(refreshTempScreen);

  ez.addEvent(refreshSystemScreen);
  
  ezMenu submenu("System");
  
  submenu.txtBig();
  
  submenu.addItem("Version Slave");                   //Version Slave
  
  submenu.addItem("Status");                          //Status

  submenu.addItem("");
  submenu.addItem("Exit");
  
  submenu.run();
}
