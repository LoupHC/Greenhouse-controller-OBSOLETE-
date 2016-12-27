#include <EEPROM.h>
#include <avr/pgmspace.h>
//---------------------LCD-------------------
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR    0x22              // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3

LiquidCrystal_I2C  lcd(I2C_ADDR,2,1,0,4,5,6,7);
//---------------------RTC--------------------
#include <DS3231.h>
DS3231  rtc(SDA, SCL);                // Init the DS3231 using the hardware interface
Time  t;     

#include <DHT.h>
#define DHTPIN            A1          // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT11       // Uncomment the type of sensor in use:
DHT dht(DHTPIN, DHTTYPE);
int yr; byte mt; byte dy; byte hr; byte mn; byte sc;

//--------------------LCD Menu----------------
//menuPin
boolean menuPinState = 1;
boolean lastMenuPinState = 1;
//curseur
byte currentMenuItem = 1;
//buttonstate
byte state = 0;
byte laststate = 0;
//menu
int menu = 0;
//#items/menu
byte Nbitems = 6;
char* menuitems[10];
//--------------Relais--------------------
//Sorties
#define ROLLUP1_POWER  12//relais on/off - moteur1
#define ROLLUP1_DIRECTION  11 //relais gauche/droite - moteur1
#define ROLLUP2_POWER  10//relais on/off - moteur2
#define ROLLUP2_DIRECTION  9 //relais gauche/droite - moteur2
#define FAN  8 //relais ventilation forcée
#define CHAUFFAGE1 7 //relais fournaise1
#define CHAUFFAGE2 6 // relais fournaise2
#define menuPin 3
//vocabulaire
const int CLOSE = HIGH;
const int OPEN = LOW;
const int ON = LOW;
const int OFF = HIGH;

//----------------------Program-----------------
//**********PROGRAMMATION PAR DÉFAUT*************
//Programmes de températures
byte HP1 = 7;                   //Heure d'exécution du premier programme
byte MP1 = 30;                  //Minutes d'exécution du premier programme
byte TEMP_CIBLEP1 = 20;         //Température cible du program 1
byte HP2 = 11;                  //Heure d'exécution du deuxième programme
byte MP2 = 0;                   //Minutes d'exécution du deuxième programme
byte TEMP_CIBLEP2 = 24;         //Température cible du program2
byte HP3 = 19;                  //Heure d'exécution du deuxième programme
byte MP3 = 30;                  //Minutes d'exécution du deuxième programme
byte TEMP_CIBLEP3 = 22;         //Température cible du programme 3
//Sorties:
byte HYST_ROLLUP = 2;           //hysteresis rollup
byte HYST_VENT = 2;             //hysteresis ventilation
byte HYST_FOURNAISE1 = 2;       //hysteresis fournaise 1
byte HYST_FOURNAISE2 = 2;       //hysteresis fournaise 2
//Modificateurs
byte rmodE = 11;                //Modificateur relais (Mod.réel = rmodE-11, donc 11 équivaut à un modificateur de 0C, 13 de 2C, 9 de -2C, etc.
byte vmodE = 13;                //Modificateur ventilation
byte f1modE = 9;                //Modificateur fournaise1
byte f2modE = 9;                //Modificateur fournaise2
//temps de rotation et pauses
byte rotation = 4;
byte pause = 2;
//***********************************************

//consignes initiales
byte opened = 0;
byte last_opened = 0;
boolean heating1 = false;                     //fournaise 1 éteinte par défaut
boolean last_heating1 = false;
boolean heating2 = false;                     //fournaise 2 éteinte par défaut
boolean last_heating2 = false;
boolean fan = false;                          //VENTilation forcée éteinte par défaut
boolean last_fan = false;
float greenhouseTemperature = 20.0;               //température par défaut : 20C (ajusté après un cycle)
float last_greenhouseTemperature = 20.0;
float greenhouseHumidity;
float last_greenhouseHumidity = 40.0;
int rmod = (rmodE-11);                        //modificateur relais
int vmod = (vmodE-11);                        //modificateur VENTilation
int f1mod = (f1modE-11);                      //modificateur fournaise1
int f2mod = (f2modE-11);                      //modificateur fournaise2
int TEMP_CIBLE = 20;                          //température cible par défaut : 20C (ajustée après un cycle))
int TEMP_ROLLUP = TEMP_CIBLE + rmod;
int TEMP_VENTILATION = TEMP_CIBLE + vmod;
int TEMP_FOURNAISE1 = TEMP_CIBLE + f1mod;
int TEMP_FOURNAISE2 = TEMP_CIBLE + f2mod;
byte PROGRAMME = 1;                           //Programme horaire par défaut
long ROTATION_TIME = (rotation*1000);         //temps de rotation des moteurs(en mili-secondes)
long PAUSE_TIME = (pause*1000);               //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)

//incréments d'ouverture(nombre d'incréments d'ouverture = PCT_OPEN/NB_OF_STEPS_IN_ANIMATION = 25/5 = 5 incréments d'ouverture)
const int PCT_OPEN = 25;
const int NB_OF_STEPS_IN_ANIMATION = 5; //doit être un divisible de PCT_OPEN
//Autres variables
const int SLEEPTIME = 1000; //temps de pause entre chaque exécution du programme(en mili-secondes)


void setup(){ 
 Serial.begin(9600);
 rtc.begin();
 dht.begin();
 lcd.begin(20, 4);
 lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
 lcd.setBacklight(HIGH);
 //newDefaultSettings();
 loadPreviousSettings();
 clearPrintTitle();

 //Définition et initalisation des sorties
  pinMode(menuPin, INPUT_PULLUP);
  pinMode(ROLLUP1_POWER, OUTPUT);
  digitalWrite(ROLLUP1_POWER, HIGH);
  pinMode(ROLLUP2_POWER, OUTPUT);
  digitalWrite(ROLLUP2_POWER, HIGH);
  pinMode(ROLLUP1_DIRECTION, OUTPUT);
  digitalWrite(ROLLUP1_DIRECTION, HIGH);
  pinMode(ROLLUP2_DIRECTION, OUTPUT);
  digitalWrite(ROLLUP2_DIRECTION, HIGH);
  pinMode(CHAUFFAGE1, OUTPUT);
  digitalWrite(CHAUFFAGE1, HIGH);
  pinMode(CHAUFFAGE2, OUTPUT);
  digitalWrite(CHAUFFAGE2, HIGH);
  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, HIGH);
  
//Remise à niveau des rollup
  Serial.println(F("Resetting position"));
  opened = 100;
  for (int i=0; i<5; i++){
    closeSides();
    delay(PAUSE_TIME);}
  Serial.println(F("Resetting done"));
  
  displayTempHRStatus();
  displayRollupStatus();
  displayFanHeaterStatus();
 }
 
void loop(){
if (digitalRead(menuPin) == LOW){
    menuPinState = 0;
    if (menuPinState != lastMenuPinState){
      displayMenu(menu);}
    Menu(menu);
    lastMenuPinState = menuPinState;
    delay(50);}
    
else if (digitalRead(menuPin) == HIGH){  
    menuPinState = 1; 
    if (menuPinState != lastMenuPinState){
      displayTempHRStatus();
      displayRollupStatus();
      displayFanHeaterStatus();
      } 
    Controle();
    lastMenuPinState = menuPinState;
    delay(SLEEPTIME);}
}
//*******************************************CONTROLE**********************************
void Controle(){
  //-----------------Horloge-----------------
  t = rtc.getTime();
  int heure = t.hour;
  int minutes = t.min;
 if (((heure >= HP1)  && (heure < HP2))  && (minutes >= MP1)){
    TEMP_CIBLE = TEMP_CIBLEP1;
    PROGRAMME = 1;
  }
  else if(((heure >= HP2)  && (heure < HP3))  && (minutes >= MP2)){
    TEMP_CIBLE = TEMP_CIBLEP2;
    PROGRAMME = 2;
  }
  else if((heure >= HP3)  && (minutes >= MP3)){
    TEMP_CIBLE = TEMP_CIBLEP3;
    PROGRAMME = 3;
  } 
  //--------------DHT11--------------------
  greenhouseTemperature = dht.readTemperature();
  greenhouseHumidity = dht.readHumidity();
//--------------Relais--------------------
//Programme d'ouverture/fermeture des rollups
  if(greenhouseTemperature < TEMP_ROLLUP-(HYST_ROLLUP/2)){
    closeSides();
  }else if(greenhouseTemperature > TEMP_ROLLUP+(HYST_ROLLUP/2)){
    openSides();
  }
//Programme fournaise1
  if (greenhouseTemperature < (TEMP_FOURNAISE1-(HYST_FOURNAISE1/2))){
    setHeater1(ON);
    digitalWrite(CHAUFFAGE1, ON);
  }else if(greenhouseTemperature > (TEMP_FOURNAISE1+(HYST_FOURNAISE1/2))){
    setHeater1(OFF);
    digitalWrite(CHAUFFAGE1, OFF);
  }
//Programme fournaise2
  if (greenhouseTemperature < (TEMP_FOURNAISE2-(HYST_FOURNAISE2/2))){
    setHeater2(ON);
    digitalWrite(CHAUFFAGE2, ON);
  }else if(greenhouseTemperature > (TEMP_FOURNAISE2+(HYST_FOURNAISE2/2))){
    setHeater2(OFF);
    digitalWrite(CHAUFFAGE2, OFF);
  }
//Programme ventilation forcée
   if (greenhouseTemperature > (TEMP_VENTILATION - (HYST_VENT/2))){
    setFan(ON);
    digitalWrite(FAN, ON);
  }else if(greenhouseTemperature < (TEMP_VENTILATION + (HYST_VENT/2))){
    setFan(OFF);
    digitalWrite(FAN, OFF);
  }
//--------------Affichage sériel--------------------
    Serial.println(F(""));
    Serial.println(F("-----------------------"));
    Serial.print(rtc.getDOWStr());
    Serial.print(F(",  "));
    // Send date
    Serial.print(rtc.getDateStr());
    Serial.print(F(" - "));
    // Send time
    Serial.println(rtc.getTimeStr());
    Serial.print(F("PROGRAMME : "));
    Serial.println(PROGRAMME);
  
    Serial.println(F("-----------------------"));    
    Serial.print(F("Temperature cible :"));
    Serial.print(TEMP_CIBLE);
    Serial.println(F(" C"));

    Serial.print(F("Temperature actuelle: "));
    Serial.print(greenhouseTemperature);
    Serial.println(" C");
    Serial.print(F("Humidite: "));
    Serial.print(greenhouseHumidity);
    Serial.println(F("%"));
    Serial.println(F("-----------------------")); 
    if(fan == false){
    Serial.println(F("FAN: OFF"));}
    else if(fan == true){
    Serial.println(F("FAN: ON"));}
    if(heating1 == false){
    Serial.println(F("HEATING1: OFF"));}
    else if(heating1 == true){
    Serial.println(F("HEATING1: ON"));}
    if(heating2 == false){
    Serial.println(F("HEATING2 : OFF"));}
    else if(heating2 == true){
    Serial.println(F("HEATING2 : ON"));}
//--------------Affichage LCD--------------------
  lcd.setCursor(0, 0);
  if((greenhouseTemperature!= last_greenhouseTemperature)||(greenhouseHumidity != last_greenhouseHumidity)){
  displayTempHRStatus();
  }
  if(opened != last_opened){
  displayRollupStatus();
  }
  if((fan != last_fan)||(heating1 != last_heating1)){
  displayFanHeaterStatus();
  }
  
last_greenhouseTemperature = greenhouseTemperature;
last_greenhouseHumidity = greenhouseHumidity;
last_opened = opened;
last_fan = fan;
last_heating1 = heating1;
last_heating2 = heating2;
}

//Affichage du % d'ouverture des rollup
void setOpenedStatus(int pctIncrease){
  opened += pctIncrease;
  if(opened < 0) {
    opened = 0;
  }else if (opened > 100){
    opened = 100;
  }
  Serial.print(F("    "));
  Serial.println(opened);
  lcd.setCursor(16, 1);
  lcd.print(opened);    
  lcd.print(F("% "));
}

//Exécution de la séquence d'ouverture/fermeture
void animate(int movement){
  for (int i=0; i < NB_OF_STEPS_IN_ANIMATION; i++){
      delay(ROTATION_TIME / NB_OF_STEPS_IN_ANIMATION);
      setOpenedStatus(movement * PCT_OPEN / NB_OF_STEPS_IN_ANIMATION);
    };
}

//Programme d'ouverture des rollup
void openSides(){
  if(opened < 100){
    Serial.println(F(""));  
    Serial.println(F("  Opening"));
    lcd.setCursor(0, 1);
    lcd.print(F("OUVERTURE... "));
    digitalWrite(ROLLUP1_POWER,ON);
    digitalWrite(ROLLUP1_DIRECTION, OPEN);
    digitalWrite(ROLLUP2_POWER,ON);
    digitalWrite(ROLLUP2_DIRECTION, OPEN);
    animate(1);
    digitalWrite(ROLLUP1_POWER,OFF);
    digitalWrite(ROLLUP2_POWER,OFF);
    Serial.println(F("  Done opening"));
    lcd.setCursor(0, 1);
    lcd.print(F("ROLLUPS:     "));
    delay(PAUSE_TIME);
  }
}

//Programme de fermeture des rollups
void closeSides(){
  if (opened > 0){
    Serial.println(F(""));  
    Serial.println(F("  Closing"));
    lcd.setCursor(0, 1);
    lcd.print(F("FERMETURE... "));
    digitalWrite(ROLLUP1_POWER,ON);
    digitalWrite(ROLLUP1_DIRECTION, CLOSE);
    digitalWrite(ROLLUP2_POWER,ON);
    digitalWrite(ROLLUP2_DIRECTION, CLOSE);
    animate(-1);
    digitalWrite(ROLLUP1_POWER, OFF);
    digitalWrite(ROLLUP2_POWER, OFF);
    Serial.println(F("  Done closing"));
    lcd.setCursor(0, 1);
    lcd.print(F("ROLLUPS:     "));
    delay(PAUSE_TIME);    
  }
}


//État de la première fournaise
void setHeater1(int heaterCommand1){
  if ((heaterCommand1 == ON) && (heating1 == false)){
    Serial.println(F(""));  
    Serial.println(F("  Start heating1"));
    heating1 = true;
  }else if ((heaterCommand1 == OFF) && (heating1 == true)){
    Serial.println(F(""));  
    Serial.println(F("  Stop heating1"));
    heating1 = false;    
    }
  }

//État de la deuxième fournaise
void setHeater2(int heaterCommand2){
  if ((heaterCommand2 == ON) && (heating2 == false)){
    Serial.println(F(""));  
    Serial.println(F("  Start heating2"));
    heating2 = true;
  }else if ((heaterCommand2 == OFF) && (heating2 == true)){
    Serial.println(F(""));  
    Serial.println(F("  Stop heating2"));
    heating2 = false;    
    }
  }

//État de la ventilation
void setFan(int fanCommand){
  if ((fanCommand == ON) && (fan == false)){
    Serial.println(F(""));  
    Serial.println(F("  Start fan"));

    fan = true;
  }else if ((fanCommand == OFF) && (fan == true)){
    Serial.println(F(""));  
    Serial.println(F("  Stop fan"));
    fan = false;
  }
}
void displayTempHRStatus(){
  lcd.setCursor(0,0);
  lcd.print(F("                    "));
  lcd.setCursor(0,0);
  lcd.print(F("T: "));
  lcd.print(greenhouseTemperature);
  lcd.print(F("C  "));
  lcd.setCursor(10,0);
  lcd.print(F("HR: "));
  lcd.print(greenhouseHumidity);
  lcd.print("%"); 
}
void displayRollupStatus(){
  lcd.setCursor(0,1);
  lcd.print(F("                    "));
  lcd.setCursor(0, 1);
  lcd.print(F("ROLLUPS:     "));
  lcd.setCursor(16, 1);
  lcd.print(opened);    
  lcd.print(F("% "));
}
void displayFanHeaterStatus(){
  lcd.setCursor(0,2);
  lcd.print(F("                    "));
  lcd.setCursor(0,3);
  lcd.print(F("                    "));
  if(fan == false){
      lcd.setCursor(0, 2);             
      lcd.print(F("FAN:"));
      lcd.setCursor(16,2);
      lcd.print(F("OFF"));}
  else if(fan == true){
      lcd.setCursor(0,2);
      lcd.print(F("FAN:"));
      lcd.setCursor(16,2);
      lcd.print(F("ON "));}
  if(heating1 == false){
      lcd.setCursor(0, 3);             
      lcd.print(F("HEATING:"));
      lcd.setCursor(16,3);
      lcd.print(F("OFF"));}
  else if(heating1 == true){
      lcd.setCursor(0,3);
      lcd.print(F("HEATING: "));
      lcd.setCursor(16,3);
      lcd.print(F("ON "));}  
}
//*******************************************EEPROM***********************************************
void newDefaultSettings(){
 EEPROM.write(0,HYST_ROLLUP); 
 EEPROM.write(1,HYST_VENT); 
 EEPROM.write(2,HYST_FOURNAISE1); 
 EEPROM.write(3,HYST_FOURNAISE2); 
 EEPROM.write(4,HP1); 
 EEPROM.write(5,MP1); 
 EEPROM.write(6,TEMP_CIBLEP1); 
 EEPROM.write(7,HP2);  
 EEPROM.write(8,MP2);  
 EEPROM.write(9,TEMP_CIBLEP2);  
 EEPROM.write(10,HP3);  
 EEPROM.write(11,MP3);  
 EEPROM.write(12,TEMP_CIBLEP3); 
 EEPROM.write(13,rotation);
 EEPROM.write(14,pause);
 EEPROM.write(15,rmodE);
 EEPROM.write(16,vmodE);
 EEPROM.write(17, f1modE);
 EEPROM.write(18, f2modE);
}

void loadPreviousSettings(){
 HYST_ROLLUP = EEPROM.read(0); 
 HYST_VENT = EEPROM.read(1); 
 HYST_FOURNAISE1 = EEPROM.read(2); 
 HYST_FOURNAISE2 = EEPROM.read(3); 
 HP1 = EEPROM.read(4); 
 MP1 = EEPROM.read(5); 
 TEMP_CIBLEP1 = EEPROM.read(6); 
 HP2 = EEPROM.read(7);  
 MP2 = EEPROM.read(8);  
 TEMP_CIBLEP2 = EEPROM.read(9);  
 HP3 = EEPROM.read(10);  
 MP3 = EEPROM.read(11);  
 TEMP_CIBLEP3 = EEPROM.read(12); 
 rotation = EEPROM.read(13);
 pause = EEPROM.read(14);
 rmodE = EEPROM.read(15);
 vmodE = EEPROM.read(16);
 f1modE = EEPROM.read(17);
 f2modE = EEPROM.read(18);
 }
 


//**************************************MENU****************************************************
void Menu(int x){
  int a = analogRead(0);
  buttonState(a);
  
  if ((state == 1) && (state != laststate)){
      currentMenuItem = currentMenuItem + 1;
      real_currentMenuItem(Nbitems);
      displayMenu(x);
  } else if ((state == 2) && (state != laststate)){
      currentMenuItem = currentMenuItem - 1;
      real_currentMenuItem(Nbitems);
      displayMenu(x);
  } else if ((state == 3) && (state != laststate)){
      real_currentMenuItem(Nbitems);
      selectMenu(x, currentMenuItem);
  }
  laststate = state;
}

void buttonState(int x){
  if(x < 800){state = 0;} 
  else if(x < 900){state = 1;} 
  else if (x < 950){state = 2;}
  else if (x < 1000){state = 3;}
}
 
void real_currentMenuItem(int x){
  if(currentMenuItem < 1){
  currentMenuItem = 1;
  }
  else if(currentMenuItem > x){
  currentMenuItem = x;
  }
}

void Scrollingmenu (int x, const char a[20] PROGMEM, const char b[20] PROGMEM, const char c[20] PROGMEM, const char d[20] PROGMEM, const char e[20] PROGMEM, const char f[20] PROGMEM, const char g[20] PROGMEM, const char h[20] PROGMEM, const char i[20] PROGMEM, const char j[20] PROGMEM, int y){
  const int numLcdRows = 3;
  byte scrollPos = 0;
  Nbitems = x;
  const char* menuitems[] PROGMEM = {a,b,c,d,e,f,g,h,i,j};
  
        if (y > numLcdRows){scrollPos = y - numLcdRows;
      } else {scrollPos = 0;
      }
      clearPrintTitle();
      for (int i=0; i < numLcdRows; ++i) {
            lcd.setCursor(0, i+1); 
            lcd.print(menuitems[i + scrollPos]);
            }
      lcd.setCursor(19, (y - scrollPos));
      lcd.blink();
}

void Scrollingnumbers(int x, int y, int z, int a){
  const int numLcdRows = 3;
  int scrollPos = 0;
  Nbitems = x;
  
      if (y > numLcdRows){scrollPos = y - numLcdRows;
      } else {scrollPos = 0;
      }
      clearPrintTitle();
      for (int i=0; i < numLcdRows; ++i) {
            lcd.setCursor(0, i+1);
            if(y < 4){
            lcd.print((z-a)+(i*a)+(a));}
            else if(y < x){
            lcd.print((z-a)+(i*a)+((y-2)*a));}
            else if (y == x){
            lcd.print("back");}
            else{}
            }
      lcd.setCursor(19, (y - scrollPos));
      lcd.blink();
}
void clearPrintTitle(){
  lcd.noBlink();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("**ROBOT-SERRE**V.1**");
}
//***********************************DISPLAY****************************************************
//Scrollingmenu(Nbitems, Item1, item2, item3, item4, item5, Item6, item7, item8, item9, item10, currentMenuItem); //leave variable "Itemx" blank if it doesnt exist
//Scrollingnumbers(Nbitems, currentMenuItem, starting number, multiplicator);

void displayMenu(int x){
  switch (x){
  case 0: Scrollingmenu(6, "Temperature/humidex","Date/time","Rollups","Ventilation","Chauffage","Programmes","","","","", currentMenuItem);break;
  case 1: Scrollingmenu(6, "Date","Time", "SetDOW", "Set date","Set time","back","","","","", currentMenuItem);break; 
  case 11: Scrollingmenu(8, "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "back","","", currentMenuItem);break; 
  case 12:Scrollingnumbers(11, currentMenuItem, 2016, 1);  break;
  case 121: Scrollingnumbers(13, currentMenuItem, 1 ,1);  break;
  case 1211: Scrollingnumbers(32, currentMenuItem, 1, 1); break;
  case 13: Scrollingnumbers(25, currentMenuItem, 1, 1);break;
  case 131: Scrollingnumbers(62, currentMenuItem, 0 , 1); break;
  case 1311: Scrollingnumbers(62, currentMenuItem, 0, 1);break;
  case 2: Scrollingmenu(6, "Etat", "Programme", "Set hysteresis", "Set rotation time(s)", "Set pause time(s)", "back","","","","", currentMenuItem);break; 
  case 21: Scrollingnumbers(6, currentMenuItem, 2, 2);break; 
  case 22: Scrollingnumbers(21, currentMenuItem, 1, 1);break; 
  case 23: Scrollingmenu(10, "5", "15", "20", "30", "45", "60", "75", "90", "120", "back", currentMenuItem);break; 
  case 3: Scrollingmenu(3, "Etat", "Set hysteresis", "back", "", "", "", "", "", "", "", currentMenuItem);break; 
  case 31: Scrollingnumbers(6, currentMenuItem, 2, 2);break; 
  case 4: Scrollingmenu(5, "(F1)Etat", "(F2)Etat","(F1)Set hysteresis", "(F2)Set hysteresis", "back", "", "", "", "", "", currentMenuItem);break; 
  case 41: Scrollingnumbers(6, currentMenuItem, 2, 2);break;  
  case 42: Scrollingnumbers(6, currentMenuItem, 2, 2);break;
  case 5: Scrollingmenu(9, "Programme 1", "Programme 2", "Programme 3", "Modificateurs", "Set Programme 1", "Set Programme 2", "Set Programme 3", "Set Modificateurs", "back","",currentMenuItem);break; 
  case 51:Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back","","","","","","","", currentMenuItem);break; 
  case 511: Scrollingnumbers(25, currentMenuItem, 1 , 1); break;
  case 5111: Scrollingnumbers(62, currentMenuItem, 0, 1);break;
  case 512: Scrollingnumbers(51, currentMenuItem, 0 , 1); break;
  case 52: Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back","","","","","","","", currentMenuItem);break; 
  case 521: Scrollingnumbers(25, currentMenuItem, 1, 1); break;
  case 5211: Scrollingnumbers(62, currentMenuItem, 0, 1);break;
  case 522: Scrollingnumbers(51, currentMenuItem, 0, 1); break;
  case 53: Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back","","","","","","","", currentMenuItem);break; 
  case 531: Scrollingnumbers(25, currentMenuItem, 1 , 1); break;
  case 5311: Scrollingnumbers(62, currentMenuItem, 0, 1);break;
  case 532: Scrollingnumbers(51, currentMenuItem, 0, 1); break;
  case 54: Scrollingmenu(3, "Mod. rollups", "Mod. ventilation", "Mod. fournaise1", "Mod. fournaise2", "back","","","","","", currentMenuItem);break; 
  case 541: Scrollingnumbers(22, currentMenuItem, -10, 1);break;
  case 542: Scrollingnumbers(22, currentMenuItem, -10, 1);break;
  case 543: Scrollingnumbers(22, currentMenuItem, -10, 1);break;
  case 544: Scrollingnumbers(22, currentMenuItem, -10, 1);break;
  }
}
//*****************************************SELECT**********************************************

void switchmenu(int x){
  delay(1000);
  menu = x;
  currentMenuItem = 1;
  Nbitems = 3;
  displayMenu(menu);
}

void selectMenu(int x, int y){
  switch (x){
  //------------------------------SelectrootMenu-------------------------------------
  //"Temperature/humidex","Date/time","Rollups","Ventilation","Chauffage"
  case 0:
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("Temperature : "));lcd.print(dht.readTemperature());lcd.print(F("C"));
         lcd.setCursor(0,2);lcd.print(F("HR : "));lcd.print(dht.readHumidity());lcd.print(F("%"));
    break;
    case 2:switchmenu(1);break;      
    case 3:switchmenu(2);break;
    case 4:switchmenu(3);break; 
    case 5:switchmenu(4);break; 
    case 6:switchmenu(5);break;
     }
  break;
  //-------------------------------SelectMenu1-----------------------------------------
  //"Date","Time","Set DOW" "Set date", "Set time","back"
  case 1 : 
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(rtc.getDOWStr());
         lcd.setCursor(0,2);lcd.print(F("Date : "));lcd.print(rtc.getDateStr());
    break;
    case 2:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("Time : "));lcd.print(rtc.getTimeStr());
    break;
    case 3:switchmenu(11);break;
    case 4:switchmenu(12);break;
    case 5:switchmenu(13);break;
    case 6:switchmenu(0);break;
    }
  break;
  //-------------------------------SelectMenu12-----------------------------------------
  //SET DAY OF THE WEEK
  case 11 :
    switch(y){
    case 1:rtc.setDOW(MONDAY);switchmenu(1);break;
    case 2:rtc.setDOW(TUESDAY);switchmenu(1);break;
    case 3:rtc.setDOW(WEDNESDAY);switchmenu(1);break;
    case 4:rtc.setDOW(THURSDAY);switchmenu(1);break;
    case 5:rtc.setDOW(FRIDAY);switchmenu(1);break;
    case 6:rtc.setDOW(SATURDAY);switchmenu(1);break;
    case 7:rtc.setDOW(SUNDAY);switchmenu(1);break;
    case 8:switchmenu(1);break;
    }
  break;
  //-------------------------------SelectMenu12-----------------------------------------
  //SET YEAR
  case 12 :   
    if(y<Nbitems){yr = (2015+y);switchmenu(121);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu131-----------------------------------------
  //SET MONTH
  case 121 :    
    if(y<Nbitems){mt = y;switchmenu(1211);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu1311-----------------------------------------
  //SET DAY
  case 1211 :   
    if(y<Nbitems){dy = y;rtc.setDate(dy,mt,yr);switchmenu(1);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu14-----------------------------------------
  //SET HOUR
  case 13 :    
    if(y<Nbitems){hr = y;switchmenu(131);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu141-----------------------------------------
  //SET MINUTES
  case 131 :    
    if(y<Nbitems){mn = y;switchmenu(1311);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu1411-----------------------------------------
  //SET SECONDS
  case 1311 :    
    if(y<Nbitems){sc = y;rtc.setTime(hr,mn,sc);switchmenu(1);}
    else{switchmenu(1);}
  break;
  //-------------------------------SelectMenu2-----------------------------------------
  //"Etat", "Programme", "Set hysteresis", "Set rotation time(s)", "Set pause time(m)", "back"
  case 2 : 
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("Ouverture : "));lcd.print(opened);lcd.print(F("%"));
         lcd.setCursor(0,2);lcd.print(F("TP : "));lcd.print(pause);lcd.print(F("s | TR :"));lcd.print(rotation);lcd.print(F("s"));
         lcd.setCursor(0,3);lcd.print(F("Hysteresis : "));lcd.print(HYST_ROLLUP);lcd.print(F("C"));
    break;
    case 2:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("Programme : "));lcd.print(PROGRAMME);
         lcd.setCursor(0,2);lcd.print(F("Temp. cible : "));lcd.print(TEMP_CIBLE);lcd.print(F("C"));
         lcd.setCursor(0,3);lcd.print(F("Temp. rollup : "));lcd.print(TEMP_ROLLUP);lcd.print(F("C"));
    break;
    case 3:switchmenu(21);break;
    case 4:switchmenu(22);break;
    case 5:switchmenu(23);break;
    case 6:switchmenu(0);break;
    }
  break;
  //-------------------------------SelectMenu21-----------------------------------------
  //SET HYSTERESIS
  case 21 : 
    if(y<Nbitems){HYST_ROLLUP = (y*2);switchmenu(2);EEPROM.write(0,HYST_ROLLUP);}
    else{switchmenu(2);}
  break;
  //-------------------------------SelectMenu22-----------------------------------------
  //SET ROTATION TIME
  case 22 :  
    if(y<Nbitems){rotation = y;switchmenu(2);EEPROM.write(13, rotation);}
    else{switchmenu(2);}
  break;
  //-------------------------------SelectMenu23-----------------------------------------
  //"5", "15", "20", "30", "45", "60", "75", "90", "120", "back"
  case 23 :
  switch(y){
    case 1:pause = 5;switchmenu(2);EEPROM.write(14, pause);break;
    case 2:pause = 15;switchmenu(2);EEPROM.write(14, pause);break;
    case 3:pause = 20;switchmenu(2);EEPROM.write(14, pause);break;
    case 4:pause = 30;switchmenu(2);EEPROM.write(14, pause);break;
    case 5:pause = 45;switchmenu(2);EEPROM.write(14, pause);break;
    case 6:pause = 60;switchmenu(2);EEPROM.write(14, pause);break;
    case 7:pause = 75;switchmenu(2);EEPROM.write(14, pause);break;
    case 8:pause = 90;switchmenu(2);EEPROM.write(14, pause);break;
    case 9:pause = 120;switchmenu(2);EEPROM.write(14, pause);break;
    case 10:switchmenu(2);break;
  }
  break;

  //-------------------------------SelectMenu3-----------------------------------------
  case 3 : 
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("FAN : "));
             if(fan == false){ lcd.print(F("OFF"));}
             else {lcd.print(F("ON"));}
         lcd.setCursor(0,2);lcd.print(F("Temp. cible : "));lcd.print(TEMP_VENTILATION);lcd.print(F("C"));
         lcd.setCursor(0,3);lcd.print(F("Hysteresis : "));lcd.print(HYST_VENT);lcd.print(F("C"));
    break;
    case 2:switchmenu(31);break;
    case 3:switchmenu(0);break;
    }
  break;
  //-------------------------------SelectMenu31-----------------------------------------
  //SET HYSTERESIS
  case 31 :   
    if(y<Nbitems){HYST_VENT = (y*2);switchmenu(3); EEPROM.write(1, HYST_VENT);}
    else{switchmenu(3);}
  break;
  //-------------------------------SelectMenu4-----------------------------------------
  case 4 : 
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("FOURNAISE(1) : "));
             if(heating1 == false){lcd.print(F("OFF"));}
             else {lcd.print(F("ON"));}
         lcd.setCursor(0,2);lcd.print(F("Temp. cible : "));lcd.print(TEMP_FOURNAISE1);lcd.print(F("C"));
         lcd.setCursor(0,3);lcd.print(F("Hysteresis : "));lcd.print(HYST_FOURNAISE1);lcd.print(F("C"));
    break;
    case 2:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("FOURNAISE(2) : "));
             if(heating1 == false){lcd.print(F("OFF"));} 
             else {lcd.print(F("ON"));}
         lcd.setCursor(0,2);lcd.print(F("Temp. cible : "));lcd.print(TEMP_FOURNAISE2);lcd.print(F("C"));
         lcd.setCursor(0,3);lcd.print(F("Hysteresis : "));lcd.print(HYST_FOURNAISE2);lcd.print(F("C"));
    break;
    case 3:switchmenu(41);break;
    case 4:switchmenu(42);break;
    case 5:switchmenu(0);break;
    }
  break;
  //-------------------------------SelectMenu41-----------------------------------------
  //SET HYSTERESIS
  case 41 :   
    if(y<Nbitems){HYST_FOURNAISE1 = (y*2);switchmenu(4);EEPROM.write(2, HYST_FOURNAISE1);}
    else{switchmenu(4);}
  break;
  //-------------------------------SelectMenu42-----------------------------------------
  //SET HYSTERESIS
  case 42 :   
    if(y<Nbitems){HYST_FOURNAISE2 = (y*2);switchmenu(4);EEPROM.write(3,HYST_FOURNAISE2);}
    else{switchmenu(4);}
  break;
  //-------------------------------SelectMenu5-----------------------------------------
  //"Programme 1", "Programme 2", "Programme 3", "Modificateurs" "Set Programme 1", "Set Programme 2", "Set Programme 3", "Set Modificateurs", "back"
  case 5 : 
    switch (y){
    case 1:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("HD: "));lcd.print(HP1);lcd.print(F(":"));lcd.print(MP1);lcd.print(F(" | HF: "));lcd.print(HP2);lcd.print(F(":"));lcd.print(MP2);
         lcd.setCursor(0,2);lcd.print(F("TEMP.CIBLE : "));lcd.print(TEMP_CIBLEP1);lcd.print(F("C"));
    break;
    case 2:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("HD: "));lcd.print(HP2);lcd.print(F(":"));lcd.print(MP2);lcd.print(F(" | HF: "));lcd.print(HP3);lcd.print(F(":"));lcd.print(MP3);
         lcd.setCursor(0,2);lcd.print(F("TEMP.CIBLE : "));lcd.print(TEMP_CIBLEP2);lcd.print(F("C"));
    break;
    case 3:
         lcd.noBlink();
         clearPrintTitle();
         lcd.setCursor(0,1);lcd.print(F("HD: "));lcd.print(HP3);lcd.print(F(":"));lcd.print(MP3);lcd.print(F(" | HF: "));lcd.print(HP1);lcd.print(F(":"));lcd.print(MP1);
         lcd.setCursor(0,2);lcd.print(F("TEMP.CIBLE : "));lcd.print(TEMP_CIBLEP3);lcd.print(F("C"));
    break;
    case 4:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0,1);lcd.print(F("TEMP.CIBLE : "));lcd.print(TEMP_CIBLE);
          lcd.setCursor(0,2);lcd.print(F("RMod:"));lcd.print(rmod);
          lcd.setCursor(8,2);lcd.print(F("| f1mod:"));lcd.print(f1mod);
          lcd.setCursor(0,3);lcd.print(F("VMod:"));lcd.print(vmod);
          lcd.setCursor(8,3);lcd.print(F("| f2mod:"));lcd.print(f2mod);
    break;
    case 5:switchmenu(51);break;
    case 6:switchmenu(52);break;
    case 7:switchmenu(53);break;
    case 8:switchmenu(54);break;
    case 9:switchmenu(0);break;
    }
  break;
  //-------------------------------SelectMenu51-----------------------------------------
  case 51:
  switch(y){
    case 1:switchmenu(511);break;
    case 2:switchmenu(512);break;
    case 3:switchmenu(5);break;
  }
  break;
  //-------------------------------SelectMenu511-----------------------------------------
  //SET HOUR
  case 511 :    
    if(y<Nbitems){HP1 = y;switchmenu(5111);EEPROM.write(4,HP1);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu5111-----------------------------------------
  //SET MINUTES
  case 5111 :    
    if(y<Nbitems){MP1 = (y-1);switchmenu(51);EEPROM.write(5,MP1);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu512-----------------------------------------
  //SET TEMP_CIBLE
  case 512 :    
    if(y<Nbitems){TEMP_CIBLEP1 = y;switchmenu(51);EEPROM.write(6,TEMP_CIBLEP1);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu52-----------------------------------------
  case 52:
  switch(y){
    case 1: switchmenu(521);break;
    case 2:switchmenu(522);break;
    case 3:switchmenu(5);break;
  }
  break;
  //-------------------------------SelectMenu521-----------------------------------------
  //SET HOUR
  case 521 :    
    if(y<Nbitems){HP2 = y;switchmenu(5211);EEPROM.write(7,HP2);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu5211-----------------------------------------
  //SET MINUTES
  case 5211 :    
    if(y<Nbitems){MP2 = (y-1);switchmenu(51);EEPROM.write(8,MP2);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu522-----------------------------------------
  //SET TEMP_CIBLE
  case 522 :    
    if(y<Nbitems){TEMP_CIBLEP2 = y;switchmenu(52);EEPROM.write(9,TEMP_CIBLEP2);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu53-----------------------------------------
  //"Heure", "Tempcible", "back"
  case 53:
  switch(y){
    case 1: switchmenu(531);break;
    case 2:switchmenu(532);break;
    case 3:switchmenu(5);break;
  }
  break;
  //-------------------------------SelectMenu531-----------------------------------------
  //SET HOUR
  case 531 :    
    if(y<25){HP3 = y;switchmenu(5311);EEPROM.write(10,HP3);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu5311-----------------------------------------
  //SET MINUTES
  case 5311 :    
    if(y<61){MP3 = (y-1);switchmenu(51);EEPROM.write(11,MP3);}
    else{switchmenu(51);}
  break;
  //-------------------------------SelectMenu532-----------------------------------------
  //SET TEMP_CIBLE
  case 532 :    
    if(y<51){TEMP_CIBLEP3 = y;switchmenu(53);EEPROM.write(12,TEMP_CIBLEP3);}
    else{switchmenu(53);}
  break;
  //-------------------------------SelectMenu54-----------------------------------------
  //"Heure", "Tempcible", "back"
  case 54:
  switch(y){
    case 1:switchmenu(541);break;
    case 2:switchmenu(542);break;
    case 3:switchmenu(543);break;
    case 4:switchmenu(544);break;
    case 5:switchmenu(5);break;
  }
  break;
  //-------------------------------SelectMenu541-----------------------------------------
  //SET MOD
  case 541 :    
    if(y<Nbitems){rmodE = y;switchmenu(54);EEPROM.write(15,rmodE);}
    else{switchmenu(54);}
  break;
  //-------------------------------SelectMenu541-----------------------------------------
  //SET MOD
  case 542 :    
    if(y<Nbitems){vmodE = y;switchmenu(54);EEPROM.write(16,vmodE);}
    else{switchmenu(54);}
  break;
  //-------------------------------SelectMenu541-----------------------------------------
  //SET MOD
  case 543 :    
    if(y<Nbitems){f1modE = y;switchmenu(54);EEPROM.write(17,f1modE);}
    else{switchmenu(54);}
  break;
  //-------------------------------SelectMenu541-----------------------------------------
  //SET MOD
  case 544 :    
    if(y<Nbitems){f2modE = y;switchmenu(54);EEPROM.write(18,f2modE);}
    else{switchmenu(54);}
  break;
  }
}






