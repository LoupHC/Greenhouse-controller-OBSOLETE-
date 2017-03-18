//*****************LIBRAIRIES************************
#include <EEPROM.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLord.h>

//*****************DEFINITIONS***********************
#define ONE_WIRE_BUS A0
#define ROLLUP_OPEN  10//relais on/off - moteur2
#define ROLLUP_CLOSE  9 //relais gauche/droite - moteur2
#define FAN  8 //relais ventilation forcée
#define CHAUFFAGE1 7 //relais fournaise1
#define CHAUFFAGE2 6 // relais fournaise2
#define menuPin 2

//*********************OBJETS************************
//---------------------LCD-----------------
#define I2C_ADDR    0x27              // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3
LiquidCrystal_I2C  lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7);
//---------------------RTC-----------------
DS3231  rtc(SDA, SCL);                // Init the DS3231 using the hardware interface
Time  t;
//--------------------DS18B20-------------
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//*************VARIABLES EEPROM***********************
//Programmes de températures
byte srmod = 60;                  //Heure d'exécution du deuxième programme
byte TEMP_CIBLEP1 = 20;         //Température cible du program 1
byte HP2 = 11;                  //Heure d'exécution du deuxième programme
byte MP2 = 0;                   //Minutes d'exécution du deuxième programme
byte TEMP_CIBLEP2 = 24;         //Température cible du program2
byte ssmod = 60;                  //Heure d'exécution du deuxième programme
byte TEMP_CIBLEP3 = 22;         //Température cible du programme 3
byte ramping = 5;
//Sorties:
byte HYST_ROLLUP = 2;           //hysteresis rollup
byte HYST_VENT = 2;             //hysteresis ventilation
byte HYST_FOURNAISE1 = 2;       //hysteresis fournaise 1
byte HYST_FOURNAISE2 = 2;       //hysteresis fournaise 2
//Modificateurs
byte rmodE = 11;                //Modificateur relais (Mod.réel = rmodE-11, donc 11 équivaut à un modificateur de 0C, 13 de 2C, 9 de -2C, etc.
byte vmodE = 13;                //Modificateur ventilation
byte f1modE = 9;                //Modificateur fournaise1
byte f2modE = 7;                //Modificateur fournaise2
//temps de rotation et pauses
byte rotation = 2;
byte pause = 1;
//*********************VARIABLES GLOBALES*************
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
//-----------------Control---------------------
//vocabulaire
const int CLOSE = LOW;
const int OPEN = HIGH;
const int ON = HIGH;
const int OFF = LOW;
//consignes initiales
int increments = 5;
int incrementCounter = 0;
int last_incrementCounter = 1;
boolean heating1 = false;                     //fournaise 1 éteinte par défaut
boolean last_heating1 = true;
boolean heating2 = false;                     //fournaise 2 éteinte par défaut
boolean last_heating2 = true;
boolean fan = false;                          //VENTilation forcée éteinte par défaut
boolean last_fan = true;
float greenhouseTemperature = 20.0;               //température par défaut : 20C (ajusté après un cycle)
float last_greenhouseTemperature;
int rmod;                      //modificateur relais
int vmod;                      //modificateur VENTilation
int f1mod;                    //modificateur fournaise1
int f2mod;                    //modificateur fournaise2

int HSR; //hour of sunrise
int MSR; //minute of sunrise
int HP1;
int MP1;
int HSS; //hour of sunset
int MSS; //minute of sunset
int HP3;
int MP3;
int SRmod;
int SSmod;
int TEMP_CIBLE = 20;
//température cible par défaut : 20C (ajustée après un cycle))
int NEW_TEMP_CIBLE;
//temp cible pour équipement chauffage/refroidissement
int TEMP_ROLLUP;
int TEMP_VENTILATION;
int TEMP_FOURNAISE1;
int TEMP_FOURNAISE2;
byte PROGRAMME = 1;                           //Programme horaire par défaut
long ROTATION_TIME;       //temps de rotation des moteurs(en mili-secondes)
long PAUSE_TIME;             //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)

//Autres variables
const int SLEEPTIME = 900; //temps de pause entre chaque exécution du programme(en mili-secondes)
//variables de temps
int yr; byte mt; byte dy; byte hr; byte mn; byte sc;
int last_day = 0;
//Timelord
const int TIMEZONE = -5; //PST
const float LATITUDE = 45.50, LONGITUDE = -73.56; // set your position here

TimeLord myLord; // TimeLord Object, Global variable
byte sunTime[]  = {0, 0, 0, 30, 12, 16};
byte sunRise[6];
byte sunSet [6];
int mSunrise, mSunset; //sunrise and sunset expressed as minute of day (0-1439)


//Ramping
unsigned long lastCount = 0;
unsigned long rampingInterval;

//**************************************************************
//*****************       SETUP      ***************************
//**************************************************************

void setup() {
  Serial.begin(9600);
  rtc.begin();
  sensors.begin();
  lcd.begin(20, 4);
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
  
  /*Pour reconfigurer les paramètres par défaut: 
   *1- Modifier les variables EEPROM 
   *2- Enlever les "//" avant newDefaultSettings, 
   *2- Uploader le programme dans la carte
   *3- Remettre les "//" avant newDefaultSettings,
   *4- Uploader le programme à nouveau
  */
  //newDefaultSettings();
  loadPreviousSettings();
  
  //Modification des variables de contrôles
  rmod = (rmodE - 11);                      //modificateur relais
  vmod = (vmodE - 11);                      //modificateur VENTilation
  f1mod = (f1modE - 11);                    //modificateur fournaise1
  f2mod = (f2modE - 11);                    //modificateur fournaise2
  SRmod = srmod-60;
  SSmod = ssmod-60;
  TEMP_ROLLUP = TEMP_CIBLE + rmod;
  TEMP_VENTILATION = TEMP_CIBLE + vmod;
  TEMP_FOURNAISE1 = TEMP_CIBLE + f1mod;
  TEMP_FOURNAISE2 = TEMP_CIBLE + f2mod;
  ROTATION_TIME = (rotation * 1000);       //temps de rotation des moteurs(en mili-secondes)
  PAUSE_TIME = (pause * 1000);             //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
  rampingInterval = ramping*60*1000;


  pinMode(A1, INPUT_PULLUP); // sets analog pin for input 
  //Définition et initalisation des sorties
  pinMode(menuPin, INPUT_PULLUP);
  pinMode(ROLLUP_OPEN, OUTPUT);
  digitalWrite(ROLLUP_OPEN, LOW);
  pinMode(ROLLUP_CLOSE, OUTPUT);
  digitalWrite(ROLLUP_CLOSE, LOW);
  pinMode(CHAUFFAGE1, OUTPUT);
  digitalWrite(CHAUFFAGE1, LOW);
  pinMode(CHAUFFAGE2, OUTPUT);
  digitalWrite(CHAUFFAGE2, LOW);
  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);

  /* TimeLord Object Initialization */
  myLord.TimeZone(TIMEZONE * 60);
  myLord.Position(LATITUDE, LONGITUDE);
  myLord.DstRules(3,2,11,1,60); // DST Rules for USA
 

  //Remise à niveau des rollup
  //Reset();
}

//**************************************************************
//******************       LOOP      ***************************
//**************************************************************

void loop() {
  //MODE MENU
  if (digitalRead(menuPin) == LOW) {
    menuPinState = 0;
    if (menuPinState != lastMenuPinState) {
      displayMenu(menu);
    }
    Menu(menu);
    lastMenuPinState = menuPinState;
    delay(50);
  }
  
  //MODE CONTROLE
  else if (digitalRead(menuPin) == HIGH) {
    menuPinState = 1;
    lcd.noBlink();
    if (menuPinState != lastMenuPinState) {
      displayTempTimeStatus();
      displayRollupStatus();
      displayFanHeaterStatus();
    }
    Controle();
    lastMenuPinState = menuPinState;
    delay(SLEEPTIME);
  }
}


//**************************************************************
//****************       MACROS      ***************************
//**************************************************************

void Reset(){
  lcd.home();
  lcd.print("Resetting...");
    Serial.println(F("Resetting position"));
  for (int i = 0; i < 5; i++) {
    closeSides();
  }
  Serial.println(F("Resetting done"));
}

//*********************EEPROM***********************************

void newDefaultSettings() {
  EEPROM.write(0, HYST_ROLLUP);
  EEPROM.write(1, HYST_VENT);
  EEPROM.write(2, HYST_FOURNAISE1);
  EEPROM.write(3, HYST_FOURNAISE2);
  EEPROM.write(4, srmod);
  EEPROM.write(6, TEMP_CIBLEP1);
  EEPROM.write(7, HP2);
  EEPROM.write(8, MP2);
  EEPROM.write(9, TEMP_CIBLEP2);
  EEPROM.write(10, ssmod);
  EEPROM.write(12, TEMP_CIBLEP3);
  EEPROM.write(13, rotation);
  EEPROM.write(14, pause);
  EEPROM.write(15, rmodE);
  EEPROM.write(16, vmodE);
  EEPROM.write(17, f1modE);
  EEPROM.write(18, f2modE);
  EEPROM.write(19, ramping);
}

void loadPreviousSettings() {
  HYST_ROLLUP = EEPROM.read(0);
  HYST_VENT = EEPROM.read(1);
  HYST_FOURNAISE1 = EEPROM.read(2);
  HYST_FOURNAISE2 = EEPROM.read(3);
  srmod = EEPROM.read(4);
  TEMP_CIBLEP1 = EEPROM.read(6);
  HP2 = EEPROM.read(7);
  MP2 = EEPROM.read(8);
  TEMP_CIBLEP2 = EEPROM.read(9);
  ssmod = EEPROM.read(10);
  TEMP_CIBLEP3 = EEPROM.read(12);
  rotation = EEPROM.read(13);
  pause = EEPROM.read(14);
  rmodE = EEPROM.read(15);
  vmodE = EEPROM.read(16);
  f1modE = EEPROM.read(17);
  f2modE = EEPROM.read(18);
  ramping = EEPROM.read(19);
}

//**********************CONTROLE********************************
void checkSunriseSunset(){
  t = rtc.getTime();
  dy = (t.date);
  mt = (t.mon);
  yr = (t.year-2000);
  delay(100);

  if (dy != last_day){

      sunTime[3] = dy; // Uses the Time library to give Timelord the current date
      sunTime[4] = mt;
      sunTime[5] = yr;
      myLord.SunRise(sunTime); // Computes Sun Rise. Prints:
      mSunrise = sunTime[2] * 60 + sunTime[1];
          setSunRise(sunTime);
          
      /* Sunset: */
      sunTime[3] = dy; // Uses the Time library to give Timelord the current date
      sunTime[4] = mt;
      sunTime[5] = yr;
      myLord.SunSet(sunTime); // Computes Sun Set. Prints:
      mSunset = sunTime[2] * 60 + sunTime[1];
          setSunSet(sunTime);
  }
}
  
void setSunRise(uint8_t * when){
      HSR = when[2];
      MSR = when[1];
      }
void setSunSet(uint8_t * when){
      HSS = when[2];
      MSS = when[1];}

void startRamping(){
  unsigned long rampingCounter = millis();
  if(rampingCounter - lastCount > rampingInterval) {
    // save the last time you blinked the LED 
    lastCount = rampingCounter;
    TEMP_CIBLE += 0,5;   
  } 
}

void convertDecimalToTime(){

  if ((MP1 > 59) && (MP1 < 120)){
    HP1 += 1;
    MP1 = MP1 - 60;
  }
  else if ((MP1 < 0) && (MP1 >= -60)){
    HP1 -= 1;
    MP1 = 60 + MP1;
  }
  if ((MP3 > 59) && (MP3 < 120)){
    HP3 += 1;
    MP3 = MP3 - 60;
  }
  else if ((MP3 < 0)&& (MP3 >= -60)){
    HP3 -= 1;
    MP3 = 60 + MP3;
  }
}


void Controle() {
  //-----------------Horloge----------------  
  checkSunriseSunset();
  SRmod = (int)srmod-60;
  SSmod = (int)ssmod-60;
  HP1 = HSR;
  MP1 = MSR + SRmod;
  HP3 = HSS;
  MP3 = MSS + SSmod;
  convertDecimalToTime();
  Serial.print("heure programme 1 : ");
  Serial.print(HP1);
  Serial.print(":");
  Serial.println(MP1);
  Serial.print("heure programme 3 : ");
  Serial.print(HP3);
  Serial.print(":");
  Serial.println(MP3);  
  
  t = rtc.getTime();
  int heures = t.hour;
  int minutes = t.min;

  if (((heures == HSR)  && (minutes >= MSR))||((heures > HSR) && (heures < HP2))||((heures == HP2)  && (minutes < MP2))){
    NEW_TEMP_CIBLE = TEMP_CIBLEP1;
    PROGRAMME = 1;
  }
  else if (((heures == HP2)  && (minutes >= MP2))||((heures > HP2) && (heures < HP3))||((heures == HP3)  && (minutes < MP2))){
    NEW_TEMP_CIBLE = TEMP_CIBLEP2;
    PROGRAMME = 2;
  }
  else if (((heures == HSS)  && (minutes >= HSS))||(heures > HSS)||(heures < HSR)||((heures == HSR)  && (minutes < MSR))){
    NEW_TEMP_CIBLE = TEMP_CIBLEP3;
    PROGRAMME = 3;
  }
  //--------------DS18B20--------------------
  sensors.requestTemperatures();
  greenhouseTemperature = sensors.getTempCByIndex(0);
  //--------------Affichage------------------
    lcdDisplay();
  //--------------Relais--------------------
  //Programme d'ouverture/fermeture des rollups
  if (greenhouseTemperature < (TEMP_ROLLUP - HYST_ROLLUP)) {
    closeSides();
  } else if (greenhouseTemperature > TEMP_ROLLUP) {
    openSides();
  }
  //Programme fournaise1
  if (greenhouseTemperature < TEMP_FOURNAISE1) {
    setHeater1(ON);
    digitalWrite(CHAUFFAGE1, ON);
  } else if (greenhouseTemperature > (TEMP_FOURNAISE1 + HYST_FOURNAISE1)) {
    setHeater1(OFF);
    digitalWrite(CHAUFFAGE1, OFF);
  }
  //Programme fournaise2
  if (greenhouseTemperature < TEMP_FOURNAISE2) {
    setHeater2(ON);
    digitalWrite(CHAUFFAGE2, ON);
  } else if (greenhouseTemperature > (TEMP_FOURNAISE2 + HYST_FOURNAISE2)) {
    setHeater2(OFF);
    digitalWrite(CHAUFFAGE2, OFF);
  }
  //Programme ventilation forcée
  if ((greenhouseTemperature > TEMP_VENTILATION)&&(incrementCounter == 100)) {
    setFan(ON);
    digitalWrite(FAN, ON);
  } else if (greenhouseTemperature < (TEMP_VENTILATION - HYST_VENT)) {
    setFan(OFF);
    digitalWrite(FAN, OFF);
  }
  //Debugging
  serialDisplay();
}

//Programme d'ouverture des rollup
void openSides() {
  if (incrementCounter < increments) {
  incrementCounter += 1;
    Serial.println(F(""));
    Serial.println(F("  Opening"));
    lcd.setCursor(0, 1);
    lcd.print(F("OUVERTURE     "));
    digitalWrite(ROLLUP_OPEN, ON);
    delay(ROTATION_TIME);
    digitalWrite(ROLLUP_OPEN, OFF);
    Serial.println(F("  Done opening"));
    lcd.setCursor(0, 1);
    lcd.print(F("ROLLUPS:  "));
    lcd.setCursor(9, 1);
    lcd.print(incrementCounter);
    delay(PAUSE_TIME);
  }
}

//Programme de fermeture des rollups
void closeSides() {
  if (incrementCounter > 0) {
    incrementCounter -= 1;
    Serial.println(F(""));
    Serial.println(F("  Closing"));
    lcd.setCursor(0, 1);
    lcd.print(F("FERMETURE     "));
    digitalWrite(ROLLUP_CLOSE, ON);
    delay(ROTATION_TIME);
    digitalWrite(ROLLUP_CLOSE, OFF);
    Serial.println(F("  Done closing"));
    lcd.setCursor(0, 1);
    lcd.print(F("ROLLUPS:  "));
    lcd.setCursor(9, 1);
    lcd.print(incrementCounter);
    delay(PAUSE_TIME);
  }
}


//État de la première fournaise
void setHeater1(int heaterCommand1) {
  if ((heaterCommand1 == ON) && (heating1 == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start heating1"));
    heating1 = true;
  } else if ((heaterCommand1 == OFF) && (heating1 == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop heating1"));
    heating1 = false;
  }
}

//État de la deuxième fournaise
void setHeater2(int heaterCommand2) {
  if ((heaterCommand2 == ON) && (heating2 == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start heating2"));
    heating2 = true;
  } else if ((heaterCommand2 == OFF) && (heating2 == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop heating2"));
    heating2 = false;
  }
}

//État de la ventilation
void setFan(int fanCommand) {
  if ((fanCommand == ON) && (fan == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start fan"));

    fan = true;
  } else if ((fanCommand == OFF) && (fan == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop fan"));
    fan = false;
  }
}
void lcdDisplay() {
  lcd.setCursor(0, 0);
  displayTempTimeStatus();
  if (incrementCounter != last_incrementCounter) {
    displayRollupStatus();
  }
  if ((fan != last_fan) || (heating1 != last_heating1)) {
    displayFanHeaterStatus();
  }
  last_incrementCounter = incrementCounter;
  last_incrementCounter = incrementCounter;
  last_fan = fan;
  last_heating1 = heating1;
  last_heating2 = heating2;
}

void displayRollupStatus() {
  lcd.setCursor(0, 1);
  lcd.print(F("                    "));
  lcd.setCursor(0, 1);
  lcd.print(F("ROLLUPS:  "));
  lcd.setCursor(9, 1);
  lcd.print(incrementCounter);
}

void displayFanHeaterStatus() {
  lcd.setCursor(0, 2);
  lcd.print(F("                    "));
  lcd.setCursor(0, 3);
  lcd.print(F("         "));
  if (fan == false) {
    lcd.setCursor(0, 3);
    lcd.print(F("FAN:"));
    lcd.setCursor(5, 3);
    lcd.print(F("OFF"));
  }
  else if (fan == true) {
    lcd.setCursor(0, 3);
    lcd.print(F("FAN:"));
    lcd.setCursor(5, 3);
    lcd.print(F("ON "));
  }
  if (heating1 == false) {
    lcd.setCursor(0, 2);
    lcd.print(F("H1:"));
    lcd.setCursor(5, 2);
    lcd.print(F("OFF |"));
  }
  else if (heating1 == true) {
    lcd.setCursor(0, 2);
    lcd.print(F("H1: "));
    lcd.setCursor(5, 2);
    lcd.print(F("ON  |"));
  }
  if (heating2 == false) {
    lcd.setCursor(11, 2);
    lcd.print(F("H2:"));
    lcd.setCursor(16, 2);
    lcd.print(F("OFF"));
  }
  else if (heating2 == true) {
    lcd.setCursor(11, 2);
    lcd.print(F("H2: "));
    lcd.setCursor(16, 2);
    lcd.print(F("ON "));
  }
}
  
void displayTempTimeStatus() {
  lcd.setCursor(0,0);
  lcd.print(F("                    "));
  lcd.setCursor(0,0);
  lcd.print(F("T:"));
  lcd.print(greenhouseTemperature);
  lcd.print(F("C |TC:"));
  lcd.setCursor(13,0);
  lcd.print(TEMP_CIBLE);
  lcd.print(F(".00C"));
  lcd.setCursor(9,3);
  lcd.print(F("|(P"));
  lcd.print(PROGRAMME);
  lcd.print(F(":"));
  lcd.setCursor(14,3);
  lcd.print(t.hour);
  lcd.print(F(":"));
  lcd.print(t.min);
  lcd.setCursor(19,3);
  lcd.print(F(")"));
}
void serialDisplay(){
//--------------Affichage sériel--------------------
  Serial.println(F(""));
  Serial.println(F("-----------------------"));
  Serial.print(rtc.getDOWStr());
  Serial.print(F(",  "));
  // Send date
  Serial.println(rtc.getDateStr());
  Serial.print(F(" - "));
  Serial.print(F("Lever du soleil : "));
  Serial.print(HSR);
  Serial.print(F(":"));
  Serial.println(MSR);
  Serial.print(F("Coucher du soleil : "));
  Serial.print(HSS);
  Serial.print(F(":"));
  Serial.println(MSS);
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
  Serial.println(F("-----------------------"));
  if (fan == false) {
    Serial.println(F("FAN: OFF"));
  }
  else if (fan == true) {
    Serial.println(F("FAN: ON"));
  }
  if (heating1 == false) {
    Serial.println(F("HEATING1: OFF"));
  }
  else if (heating1 == true) {
    Serial.println(F("HEATING1: ON"));
  }
  if (heating2 == false) {
    Serial.println(F("HEATING2 : OFF"));
  }
  else if (heating2 == true) {
    Serial.println(F("HEATING2 : ON"));
  }
}


//******************MENU****************************************

void Menu(int x) {
  int a = analogRead(A1);
  Serial.println(a);
  buttonState(a);

  if ((state == 3) && (state != laststate)) {
    currentMenuItem = currentMenuItem + 1;
    real_currentMenuItem(Nbitems);
    displayMenu(x);
  } else if ((state == 2) && (state != laststate)) {
    currentMenuItem = currentMenuItem - 1;
    real_currentMenuItem(Nbitems);
    displayMenu(x);
  } else if ((state == 1) && (state != laststate)) {
    real_currentMenuItem(Nbitems);
    selectMenu(x, currentMenuItem);
  }
  laststate = state;
}

void buttonState(int x) {
  if (x < 50) {
    state = 0;
  }
  else if (x < 80) {
    state = 1;
  }
  else if (x < 100) {
    state = 2;
  }
  else if (x < 120) {
    state = 3;
  }
  else {
    state = 0;
  }
}

void real_currentMenuItem(int x) {
  if (currentMenuItem < 1) {
    currentMenuItem = 1;
  }
  else if (currentMenuItem > x) {
    currentMenuItem = x;
  }
}

void Scrollingmenu (int x, const char a[20] PROGMEM, const char b[20] PROGMEM, const char c[20] PROGMEM, const char d[20] PROGMEM, const char e[20] PROGMEM, const char f[20] PROGMEM, const char g[20] PROGMEM, const char h[20] PROGMEM, const char i[20] PROGMEM, const char j[20] PROGMEM, int y) {
  const int numLcdRows = 3;
  byte scrollPos = 0;
  Nbitems = x;
  const char* menuitems[] PROGMEM = {a, b, c, d, e, f, g, h, i, j};

  if (y > numLcdRows) {
    scrollPos = y - numLcdRows;
  } else {
    scrollPos = 0;
  }
  clearPrintTitle();
  for (int i = 0; i < numLcdRows; ++i) {
    lcd.setCursor(0, i + 1);
    lcd.print(menuitems[i + scrollPos]);
  }
  lcd.setCursor(19, (y - scrollPos));
  lcd.blink();
}

void Scrollingnumbers(int x, int y, int z, int a) {
  const int numLcdRows = 3;
  int scrollPos = 0;
  Nbitems = x;

  if (y > numLcdRows) {
    scrollPos = y - numLcdRows;
  } else {
    scrollPos = 0;
  }
  clearPrintTitle();
  for (int i = 0; i < numLcdRows; ++i) {
    lcd.setCursor(0, i + 1);
    if (y < 4) {
      lcd.print((z - a) + (i * a) + (a));
    }
    else if (y < x) {
      lcd.print((z - a) + (i * a) + ((y - 2)*a));
    }
    else if (y == x) {
      lcd.print("back");
    }
    else {}
  }
  lcd.setCursor(19, (y - scrollPos));
  lcd.blink();
}
void clearPrintTitle() {
  lcd.noBlink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("**ROBOT-SERRE**V.1**");
}
//-----------------DISPLAY-------------------------
//Scrollingmenu(Nbitems, Item1, item2, item3, item4, item5, Item6, item7, item8, item9, item10, currentMenuItem); //leave variable "Itemx" blank if it doesnt exist
//Scrollingnumbers(Nbitems, currentMenuItem, starting number, multiplicator);

void displayMenu(int x) {
  switch (x) {
    case 0: Scrollingmenu(6, "Capteurs", "Date/time", "Rollups", "Ventilation", "Chauffage", "Programmes", "", "", "", "", currentMenuItem); break;
    case 1: Scrollingmenu(6, "Date", "Time", "SetDOW", "Set date", "Set time", "back", "", "", "", "", currentMenuItem); break;
    case 11: Scrollingmenu(8, "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "back", "", "", currentMenuItem); break;
    case 12: Scrollingnumbers(11, currentMenuItem, 2016, 1);  break;
    case 121: Scrollingnumbers(13, currentMenuItem, 1 , 1);  break;
    case 1211: Scrollingnumbers(32, currentMenuItem, 1, 1); break;
    case 13: Scrollingnumbers(25, currentMenuItem, 1, 1); break;
    case 131: Scrollingnumbers(62, currentMenuItem, 0 , 1); break;
    case 1311: Scrollingnumbers(62, currentMenuItem, 0, 1); break;
    case 2: Scrollingmenu(6, "Etat", "Programme", "Set hysteresis", "Set rotation time(s)", "Set pause time(s)", "back", "", "", "", "", currentMenuItem); break;
    case 21: Scrollingnumbers(6, currentMenuItem, 1, 1); break;
    case 22: Scrollingnumbers(21, currentMenuItem, 1, 1); break;
    case 23: Scrollingmenu(10, "5", "15", "20", "30", "45", "60", "75", "90", "120", "back", currentMenuItem); break;
    case 3: Scrollingmenu(3, "Etat", "Set hysteresis", "back", "", "", "", "", "", "", "", currentMenuItem); break;
    case 31: Scrollingnumbers(6, currentMenuItem, 1, 1); break;
    case 4: Scrollingmenu(5, "(F1)Etat", "(F2)Etat", "(F1)Set hysteresis", "(F2)Set hysteresis", "back", "", "", "", "", "", currentMenuItem); break;
    case 41: Scrollingnumbers(6, currentMenuItem, 1, 1); break;
    case 42: Scrollingnumbers(6, currentMenuItem, 1, 1); break;
    case 5: Scrollingmenu(10, "Programme 1", "Programme 2", "Programme 3", "Modificateurs", "Set Programme 1", "Set Programme 2", "Set Programme 3", "Set Modificateurs", "Set Ramping", "back", currentMenuItem); break;
    case 51: Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back", "", "", "", "", "", "", "", currentMenuItem); break;
    case 511: Scrollingnumbers(121, currentMenuItem, -60, 1); break;
    case 512: Scrollingnumbers(51, currentMenuItem, 0 , 1); break;
    case 52: Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back", "", "", "", "", "", "", "", currentMenuItem); break;
    case 521: Scrollingnumbers(25, currentMenuItem, 1, 1); break;
    case 5211: Scrollingnumbers(62, currentMenuItem, 0, 1); break;
    case 522: Scrollingnumbers(51, currentMenuItem, 0, 1); break;
    case 53: Scrollingmenu(3, "Set Heure", "Set Temp. cible", "back", "", "", "", "", "", "", "", currentMenuItem); break;
    case 531: Scrollingnumbers(121, currentMenuItem, -60, 1); break;
    case 532: Scrollingnumbers(51, currentMenuItem, 0, 1); break;
    case 54: Scrollingmenu(5, "Mod. rollups", "Mod. ventilation", "Mod. fournaise1", "Mod. fournaise2", "back", "", "", "", "", "", currentMenuItem); break;
    case 541: Scrollingnumbers(22, currentMenuItem, -10, 1); break;
    case 542: Scrollingnumbers(22, currentMenuItem, -10, 1); break;
    case 543: Scrollingnumbers(22, currentMenuItem, -10, 1); break;
    case 544: Scrollingnumbers(22, currentMenuItem, -10, 1); break;
    case 55: Scrollingnumbers(62, currentMenuItem, 0, 1); break;
  }
}
//------------------------SELECT-------------------

void switchmenu(int x) {
  delay(1000);
  menu = x;
  currentMenuItem = 1;
  Nbitems = 3;
  displayMenu(menu);
}

void selectMenu(int x, int y) {
  switch (x) {
    //------------------------------SelectrootMenu-------------------------------------
    //"Temperature/humidex","Date/time","Rollups","Ventilation","Chauffage"
    case 0:
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Sonde temp. : ")); lcd.print(sensors.getTempCByIndex(0)); lcd.print(F("C"));
          break;
        case 2: switchmenu(1); break;
        case 3: switchmenu(2); break;
        case 4: switchmenu(3); break;
        case 5: switchmenu(4); break;
        case 6: switchmenu(5); break;
      }
      break;
    //-------------------------------SelectMenu1-----------------------------------------
    //"Date","Time","Set DOW" "Set date", "Set time","back"
    case 1 :
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(rtc.getDOWStr());
          lcd.setCursor(0, 2); lcd.print(F("Date : ")); lcd.print(rtc.getDateStr());
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Time : ")); lcd.print(rtc.getTimeStr());
          break;
        case 3: switchmenu(11); break;
        case 4: switchmenu(12); break;
        case 5: switchmenu(13); break;
        case 6: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu12-----------------------------------------
    //SET DAY OF THE WEEK
    case 11 :
      switch (y) {
        case 1: rtc.setDOW(MONDAY); switchmenu(1); break;
        case 2: rtc.setDOW(TUESDAY); switchmenu(1); break;
        case 3: rtc.setDOW(WEDNESDAY); switchmenu(1); break;
        case 4: rtc.setDOW(THURSDAY); switchmenu(1); break;
        case 5: rtc.setDOW(FRIDAY); switchmenu(1); break;
        case 6: rtc.setDOW(SATURDAY); switchmenu(1); break;
        case 7: rtc.setDOW(SUNDAY); switchmenu(1); break;
        case 8: switchmenu(1); break;
      }
      break;
    //-------------------------------SelectMenu12-----------------------------------------
    //SET YEAR
    case 12 :
      if (y < Nbitems) {
        yr = (2015 + y);
        switchmenu(121);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu131-----------------------------------------
    //SET MONTH
    case 121 :
      if (y < Nbitems) {
        mt = y;
        switchmenu(1211);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu1311-----------------------------------------
    //SET DAY
    case 1211 :
      if (y < Nbitems) {
        dy = y;
        rtc.setDate(dy, mt, yr);
        switchmenu(1);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu14-----------------------------------------
    //SET HOUR
    case 13 :
      if (y < Nbitems) {
        hr = y;
        switchmenu(131);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu141-----------------------------------------
    //SET MINUTES
    case 131 :
      if (y < Nbitems) {
        mn = y - 1;
        switchmenu(1311);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu1411-----------------------------------------
    //SET SECONDS
    case 1311 :
      if (y < Nbitems) {
        sc = y - 1;
        rtc.setTime(hr, mn, sc);
        switchmenu(1);
      }
      else {
        switchmenu(1);
      }
      break;
    //-------------------------------SelectMenu2-----------------------------------------
    //"Etat", "Programme", "Set hysteresis", "Set rotation time(s)", "Set pause time(m)", "back"
    case 2 :
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Ouverture : ")); lcd.print(incrementCounter); lcd.print(F("%"));
          lcd.setCursor(0, 2); lcd.print(F("TP : ")); lcd.print(pause); lcd.print(F("s | TR :")); lcd.print(rotation); lcd.print(F("s"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(HYST_ROLLUP); lcd.print(F("C"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Programme : ")); lcd.print(PROGRAMME);
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(TEMP_CIBLE); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Temp. rollup : ")); lcd.print(TEMP_ROLLUP); lcd.print(F("C"));
          break;
        case 3: switchmenu(21); break;
        case 4: switchmenu(22); break;
        case 5: switchmenu(23); break;
        case 6: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu21-----------------------------------------
    //SET HYSTERESIS
    case 21 :
      if (y < Nbitems) {
        HYST_ROLLUP = y;
        switchmenu(2);
        EEPROM.write(0, HYST_ROLLUP);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu22-----------------------------------------
    //SET ROTATION TIME
    case 22 :
      if (y < Nbitems) {
        rotation = y;
        ROTATION_TIME = (rotation * 1000);
        switchmenu(2);
        EEPROM.write(13, rotation);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu23-----------------------------------------
    //"5", "15", "20", "30", "45", "60", "75", "90", "120", "back"
    case 23 :
      switch (y) {
        case 1: pause = 5; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000);break;
        case 2: pause = 15; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 3: pause = 20; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 4: pause = 30; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 5: pause = 45; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 6: pause = 60; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 7: pause = 75; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 8: pause = 90; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 9: pause = 120; switchmenu(2); EEPROM.write(14, pause); PAUSE_TIME = (pause * 1000); break;
        case 10: switchmenu(2); break;
      }
      break;

    //-------------------------------SelectMenu3-----------------------------------------
    case 3 :
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("FAN : "));
          if (fan == false) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(TEMP_VENTILATION); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(HYST_VENT); lcd.print(F("C"));
          break;
        case 2: switchmenu(31); break;
        case 3: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu31-----------------------------------------
    //SET HYSTERESIS
    case 31 :
      if (y < Nbitems) {
        HYST_VENT = y;
        switchmenu(3);
        EEPROM.write(1, HYST_VENT);
      }
      else {
        switchmenu(3);
      }
      break;
    //-------------------------------SelectMenu4-----------------------------------------
    case 4 :
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("FOURNAISE(1) : "));
          if (heating1 == false) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(TEMP_FOURNAISE1); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(HYST_FOURNAISE1); lcd.print(F("C"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("FOURNAISE(2) : "));
          if (heating1 == false) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(TEMP_FOURNAISE2); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(HYST_FOURNAISE2); lcd.print(F("C"));
          break;
        case 3: switchmenu(41); break;
        case 4: switchmenu(42); break;
        case 5: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu41-----------------------------------------
    //SET HYSTERESIS
    case 41 :
      if (y < Nbitems) {
        HYST_FOURNAISE1 = y;
        switchmenu(4);
        EEPROM.write(2, HYST_FOURNAISE1);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu42-----------------------------------------
    //SET HYSTERESIS
    case 42 :
      if (y < Nbitems) {
        HYST_FOURNAISE2 = y;
        switchmenu(4);
        EEPROM.write(3, HYST_FOURNAISE2);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu5-----------------------------------------
    //"Programme 1", "Programme 2", "Programme 3", "Modificateurs" "Set Programme 1", "Set Programme 2", "Set Programme 3", "Set Modificateurs", "back"
    case 5 :
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(HP1); lcd.print(F(":")); lcd.print(MP1); lcd.print(F(" | HF: ")); lcd.print(HP2); lcd.print(F(":")); lcd.print(MP2);
          lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLEP1); lcd.print(F("C"));
          lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(HP2); lcd.print(F(":")); lcd.print(MP2); lcd.print(F(" | HF: ")); lcd.print(HP3); lcd.print(F(":")); lcd.print(MP3);
          lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLEP2); lcd.print(F("C"));
          lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 3:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(HP3); lcd.print(F(":")); lcd.print(MP3); lcd.print(F(" | HF: ")); lcd.print(HP1); lcd.print(F(":")); lcd.print(MP1);
          lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLEP3); lcd.print(F("C"));
          lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 4:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLE);
          lcd.setCursor(0, 2); lcd.print(F("RMod:")); lcd.print(rmod);
          lcd.setCursor(8, 2); lcd.print(F("| f1mod:")); lcd.print(f1mod);
          lcd.setCursor(0, 3); lcd.print(F("VMod:")); lcd.print(vmod);
          lcd.setCursor(8, 3); lcd.print(F("| f2mod:")); lcd.print(f2mod);
          break;
        case 5: switchmenu(51); break;
        case 6: switchmenu(52); break;
        case 7: switchmenu(53); break;
        case 8: switchmenu(54); break;
        case 9: switchmenu(55); break; 
        case 10: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu51-----------------------------------------
    case 51:
      switch (y) {
        case 1: switchmenu(511);break;
        case 2: switchmenu(512); break;
        case 3: switchmenu(5); break;
      }
      break;
    //-------------------------------SelectMenu511-----------------------------------------
    //SET HOUR
    case 511 :
      if (y < Nbitems) {
        SRmod = y-60;
        srmod = y;
        EEPROM.write(4, srmod);
        switchmenu(51);
      }
      else {
        switchmenu(51);
      }
      break;
    //-------------------------------SelectMenu512-----------------------------------------
    //SET TEMP_CIBLE
    case 512 :
      if (y < Nbitems) {
        TEMP_CIBLEP1 = y;
        switchmenu(51);
        EEPROM.write(6, TEMP_CIBLEP1);
      }
      else {
        switchmenu(51);
      }
      break;
    //-------------------------------SelectMenu52-----------------------------------------
    case 52:
      switch (y) {
        case 1: switchmenu(521);break;
        case 2: switchmenu(522); break;
        case 3: switchmenu(5); break;
      }
      break;
    //-------------------------------SelectMenu521-----------------------------------------
    //SET HOUR
    case 521 :
      if (y < Nbitems) {
        HP2 = y;
        switchmenu(5211);
        EEPROM.write(7, HP2);
      }
      else {
        switchmenu(52);
      }
      break;
    //-------------------------------SelectMenu5211-----------------------------------------
    //SET MINUTES
    case 5211 :
      if (y < Nbitems) {
        MP2 = (y - 1);
        switchmenu(52);
        EEPROM.write(8, MP2);
      }
      else {
        switchmenu(52);
      }
      break;
    //-------------------------------SelectMenu522-----------------------------------------
    //SET TEMP_CIBLE
    case 522 :
      if (y < Nbitems) {
        TEMP_CIBLEP2 = y;
        switchmenu(52);
        EEPROM.write(9, TEMP_CIBLEP2);
      }
      else {
        switchmenu(52);
      }
      break;
    //-------------------------------SelectMenu53-----------------------------------------
    //"Heure", "Tempcible", "back"
    case 53:
      switch (y) {
        case 1: switchmenu(531); break;
        case 2: switchmenu(532); break;
        case 3: switchmenu(5); break;
      }
      break;
    //-------------------------------SelectMenu531-----------------------------------------
    //SET HOUR
    case 531 :
      if (y < Nbitems) {
        SSmod = y - 60;
        ssmod = y;
        EEPROM.write(10, ssmod);
        switchmenu(53);
      }
      else {
        switchmenu(53);
      }
      break;
    //-------------------------------SelectMenu532-----------------------------------------
    //SET TEMP_CIBLE
    case 532 :
      if (y < Nbitems) {
        TEMP_CIBLEP3 = y;
        switchmenu(53);
        EEPROM.write(12, TEMP_CIBLEP3);
      }
      else {
        switchmenu(53);
      }
      break;
    //-------------------------------SelectMenu54-----------------------------------------
    //
    case 54:
      switch (y) {
        case 1: switchmenu(541); break;
        case 2: switchmenu(542); break;
        case 3: switchmenu(543); break;
        case 4: switchmenu(544); break;
        case 5: switchmenu(5); break;
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 541 :
      if (y < Nbitems) {
        rmodE = y;
        rmod = (y-11);
        switchmenu(54);
        EEPROM.write(15, rmodE);
        TEMP_ROLLUP = TEMP_CIBLE + rmod;
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 542 :
      if (y < Nbitems) {
        vmodE = y;
        vmod = (y-11);
        TEMP_VENTILATION = TEMP_CIBLE + vmod;
        switchmenu(54);
        EEPROM.write(16, vmodE);
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 543 :
      if (y < Nbitems) {
        f1modE = y;
        f1mod = (y-11);
        TEMP_FOURNAISE1 = TEMP_CIBLE + f1mod;
        switchmenu(54);
        EEPROM.write(17, f1modE);
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 544 :
      if (y < Nbitems) {
        f2modE = y;
        f2mod = (y-11);
        TEMP_FOURNAISE2 = TEMP_CIBLE + f2mod;
        switchmenu(54);
        EEPROM.write(18, f2modE);
      }
      else {
        switchmenu(54);
      }
      break;//-------------------------------SelectMenu541-----------------------------------------
    //SET ramping interval
    case 55:
    if (y < Nbitems) {
        ramping = y-1;
        rampingInterval = (y-1)*60*1000;
        EEPROM.write(19, ramping);
        switchmenu(5);
      }
      else {
        switchmenu(5);
      }
      break;
  }
  
}


