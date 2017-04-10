#include <Arduino.h>

//*****************LIBRAIRIES************************
#include <MemoryFree.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLord.h>

//*****************DEFINITIONS***********************
#define LCDKEYPAD A0
#define ONE_WIRE_BUS A1
#define ROLLUP_OPEN  4//relais on/off - moteur2
#define ROLLUP_CLOSE  5 //relais gauche/droite - moteur2
#define FAN  8 //relais ventilation forcée
#define CHAUFFAGE1 7 //relais fournaise1
#define CHAUFFAGE2 6 // relais fournaise2
#define menuPin 3
#define INTERRUPT_SWITCH 2

#define HEURE 2
#define MINUTE 1

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
//-------------------Timelord-------------
const int TIMEZONE = -5; //PST
const float LATITUDE = 45.50, LONGITUDE = -73.56; // set your position here

TimeLord myLord; // TimeLord Object, Global variable


//*************VARIABLES EEPROM***********************
//Programmes de températures
byte srmod = 50;                  //Heure d'exécution du deuxième programme
byte TEMP_CIBLEP1 = 22;         //Température cible du program 1
byte HP2 = 11;                  //Heure d'exécution du deuxième programme
byte MP2 = 0;                   //Minutes d'exécution du deuxième programme
byte TEMP_CIBLEP2 = 24;         //Température cible du program2
byte ssmod = 70;                  //Heure d'exécution du deuxième programme
byte TEMP_CIBLEP3 = 18;         //Température cible du programme 3
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

//-----------------Control---------------------

boolean rollups = false;
boolean heater[2] = {true, true};
boolean cooler[1] = {true};

//initialisation
boolean firstOpening = true;
//état des sorties
boolean heating[2] = {false, false};                     //fournaise 1 éteinte par défaut
boolean fan[1] = {false};                          //VENTilation forcée éteinte par défaut
//Température
float greenhouseTemperature = 20.0;               //température par défaut : 20C (ajusté après un cycle)
float TEMP_CIBLE;
int rmod;                      //modificateur relais
int vmod;                      //modificateur VENTilation
int f1mod;                    //modificateur fournaise1
int f2mod;                    //modificateur fournaise2
float TEMP_ROLLUP;
float TEMP_VENTILATION;
float TEMP_FOURNAISE1;
float TEMP_FOURNAISE2;
//Temps de rotation et de pause des moteurs
long ROTATION_TIME;       //temps de rotation des moteurs(en mili-secondes)
long PAUSE_TIME;             //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
int increments = 5;
int incrementCounter;
//Programme horaire
byte PROGRAMME;
//variables de temps
byte sunTime[6];
byte sunRise[6];
byte sunSet [6];
int SRmod;
int SSmod;
byte P1[2];
byte P2[2];
byte P3[2];

//Ramping
float NEW_TEMP_CIBLE;
unsigned long lastCount = 0;
unsigned long rampingInterval;

//vocabulaire
const int CLOSE = LOW;
const int OPEN = HIGH;
const int ON = HIGH;
const int OFF = LOW;
//Autres variables
const int SLEEPTIME = 1000; //temps de pause entre chaque exécution du programme(en mili-secondes)

//--------------------LCD Menu----------------
//menuPin
boolean menuPinState = 1;
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
int yr; byte mt; byte dy; byte hr; byte mn; byte sc;


//**************************************************************
//*****************       SETUP      ***************************
//**************************************************************

void setup() {
  //Initialisation (moniteur série, horloge, capteurs, affichage, données astronomiques)

  Serial.begin(9600);
  rtc.begin();
  sensors.begin();
  initLCD();
  initTimeLord();
  /*Pour reconfigurer les paramètres par défaut:
   *1- Modifier les variables EEPROM
   *2- Enlever les "//" avant newDefaultSettings,
   *2- Uploader le programme dans la carte
   *3- Remettre les "//" avant newDefaultSettings,
   *4- Uploader le programme à nouveau
  */
  //newDefaultSettings();
  //Lecture des derniers paramètres enregistrés
  loadPreviousSettings();
  //Mise à jour des variables et définition des I/Os
  convertEEPROMData();
  getDateAndTime();
  //Définition des variables de temps
  setSunriseSunSet();
  getDateAndTime();
  setProgram();
  
  switch(PROGRAMME){
    case 1: TEMP_CIBLE = TEMP_CIBLEP1; break;
    case 2: TEMP_CIBLE = TEMP_CIBLEP2; break;
    case 3: TEMP_CIBLE = TEMP_CIBLEP3; break;
  }
  
  setOutputsTempCible();
  setIOS();
}

//**************************************************************
//******************       LOOP      ***************************
//**************************************************************

void loop() {
  //MODE MENU
  if (digitalRead(menuPin) == LOW) {
    Menu(menu);
    delay(50);
  }

  //MODE CONTROLE
  else if (digitalRead(menuPin) == HIGH) {
    //Protocole de controle
    checkSunriseSunset();
    setProgram();
    startRamping();
    getTemperature();
    lcdDisplay();
    //serialDisplay();
    relayLogic();
    //Pause entre chaque cycle
    delay(SLEEPTIME);
  }
}


//**************************************************************
//****************    MACROS - SETUP     ***********************
//**************************************************************

void initLCD(){
  lcd.begin(20, 4);
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
}
void initTimeLord(){
  myLord.TimeZone(TIMEZONE * 60);
  myLord.Position(LATITUDE, LONGITUDE);
  myLord.DstRules(3,2,11,1,60); // DST Rules for USA
}

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

void setIOS(){
  //Definition des entrées
  pinMode(LCDKEYPAD, INPUT_PULLUP);
  //Définition et initalisation des sorties
  pinMode(INTERRUPT_SWITCH, INPUT_PULLUP);
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
}





//**************************************************************
//****************    MACROS - CONTROLE     ********************
//**************************************************************

void checkSunriseSunset(){
  t = rtc.getTime();
  int actual_day = t.date;

  if (sunTime[3] != actual_day){
      setSunriseSunSet();
  }
  getDateAndTime();
}

void setProgram(){
  P1[HEURE] = sunRise[HEURE];
  P1[MINUTE] = sunRise[MINUTE] + SRmod;
  convertDecimalToTime(&P1[HEURE], &P1[MINUTE]);
  P3[HEURE] = sunSet[HEURE];
  P3[MINUTE] = sunSet[MINUTE] + SSmod;
  convertDecimalToTime(&P3[HEURE], &P3[MINUTE]);
  if (((sunTime[HEURE] == P1[HEURE])  && (sunTime[MINUTE] >= P1[MINUTE]))||((sunTime[HEURE] > P1[HEURE]) && (sunTime[HEURE] < P2[HEURE]))||((sunTime[HEURE] == P2[HEURE])  && (sunTime[MINUTE] < P2[MINUTE]))){
    PROGRAMME = 1;
  }
  else if (((sunTime[HEURE] == P2[HEURE])  && (sunTime[MINUTE] >= P2[MINUTE]))||((sunTime[HEURE] > P2[HEURE]) && (sunTime[HEURE] < P3[HEURE]))||((sunTime[HEURE] == P3[HEURE])  && (sunTime[MINUTE] < P2[MINUTE]))){
    PROGRAMME = 2;
  }
  else if (((sunTime[HEURE] == P3[HEURE])  && (sunTime[MINUTE] >= P3[HEURE]))||(sunTime[HEURE] > P3[HEURE])||(sunTime[HEURE] < P1[HEURE])||((sunTime[HEURE] == P1[HEURE])  && (sunTime[MINUTE] < P1[MINUTE]))){
    PROGRAMME = 3;
  }
}
//Programme de courbe de température
void startRamping(){

  switch(PROGRAMME){
    case 1: NEW_TEMP_CIBLE = TEMP_CIBLEP1; break;
    case 2: NEW_TEMP_CIBLE = TEMP_CIBLEP2; break;
    case 3: NEW_TEMP_CIBLE = TEMP_CIBLEP3; break;
  }

  if (NEW_TEMP_CIBLE > TEMP_CIBLE){
    unsigned long rampingCounter = millis();
    if(rampingCounter - lastCount > rampingInterval) {
      lastCount = rampingCounter;
      TEMP_CIBLE += 0.5;
    }
  }
  else if (NEW_TEMP_CIBLE < TEMP_CIBLE){
    unsigned long rampingCounter = millis();
    if(rampingCounter - lastCount > rampingInterval) {
      lastCount = rampingCounter;
      TEMP_CIBLE -= 0.5;
    }
  }
}

void getTemperature(){
  sensors.requestTemperatures();
  greenhouseTemperature = sensors.getTempCByIndex(0);
}

void lcdDisplay() {
  lcd.noBlink();
  //Si passe du mode menu au mode controle...
  if (menuPinState == 0) {
    lcdPrintTemp();
    lcdPrintTime();
    lcdPrintRollups();
    lcdPrintOutputsStatus();
    menuPinState = 1;
  }
  //Séquence normale...
  lcdPrintTemp();
  lcdPrintTime();
  lcdPrintRollups();
  lcdPrintOutputsStatus();
}

void serialDisplay(){
//--------------Affichage sériel--------------------
  Serial.println(F(""));
  Serial.println(F("-----------------------"));
  // Send date
  Serial.print(rtc.getDOWStr()); Serial.print(F(",  ")); Serial.println(rtc.getDateStr());
  Serial.print(F("Lever du soleil : "));   Serial.print(sunRise[HEURE]); Serial.print(F(":")); Serial.println(sunRise[MINUTE]);
  Serial.print(F("Coucher du soleil : ")); Serial.print(sunSet[HEURE]); Serial.print(F(":")); Serial.println(sunSet[MINUTE]);
  // Send time
  serialPrintDigits(sunTime[HEURE]); Serial.print(sunTime[HEURE]); Serial.print(F(":")); serialPrintDigits(sunTime[MINUTE]); Serial.println(sunTime[MINUTE]);
  Serial.print(F("PROGRAMME : "));  Serial.println(PROGRAMME);

  Serial.println(F("-----------------------"));
  Serial.print(F("Temperature cible :")); Serial.print(TEMP_CIBLE); Serial.println(F(" C"));
  Serial.print(F("Nouvelle temperature cible :")); Serial.print(NEW_TEMP_CIBLE); Serial.println(F(" C"));

  Serial.print(F("Temperature actuelle: ")); Serial.print(greenhouseTemperature); Serial.println(" C");
  Serial.println(F("-----------------------"));
  if (fan[0] == false) {    Serial.println(F("FAN: OFF"));  }
  else if (fan[0] == true) {    Serial.println(F("FAN: ON"));  }
  if (heating[0] == false) {    Serial.println(F("HEATING1: OFF"));  }
  else if (heating[0] == true) {    Serial.println(F("HEATING1: ON"));  }
  if (heating[1] == false) {    Serial.println(F("HEATING2 : OFF"));  }
  else if (heating[1] == true) {    Serial.println(F("HEATING2 : ON"));  }
}

void relayLogic(){
  lcd.noBlink();
  //Programme d'ouverture/fermeture des rollups
  setOutputsTempCible();
  if (rollups == true){
    if((incrementCounter == 6)||(incrementCounter<0)){Serial.println("ERROR");}
    if (greenhouseTemperature < (TEMP_ROLLUP - HYST_ROLLUP)) {
      closeSides();
    } else if (greenhouseTemperature > TEMP_ROLLUP) {
      openSides();
    }
  }

  //Programme fournaise1
  if (heater[0] == true){
    if (greenhouseTemperature < TEMP_FOURNAISE1) {
      //setHeater(0, ON);
      digitalWrite(CHAUFFAGE1, ON);
    } else if (greenhouseTemperature > (TEMP_FOURNAISE1 + HYST_FOURNAISE1)) {
      //setHeater(0, OFF);
      digitalWrite(CHAUFFAGE1, OFF);
    }
  }

  //Programme fournaise2
  if (heater[1] == true){
    if (greenhouseTemperature < TEMP_FOURNAISE2) {
      //setHeater(1, ON);
      digitalWrite(CHAUFFAGE2, ON);
    } else if (greenhouseTemperature > (TEMP_FOURNAISE2 + HYST_FOURNAISE2)) {
      //setHeater(1, OFF);
      digitalWrite(CHAUFFAGE2, OFF);
    }
  }
  //Programme ventilation forcée
  if (cooler[0] == true){
    if ((greenhouseTemperature > TEMP_VENTILATION)&&(digitalRead(INTERRUPT_SWITCH) == 0)){
      //setFan(0, ON);
      digitalWrite(FAN, ON);
    } else if ((greenhouseTemperature < (TEMP_VENTILATION - HYST_VENT))||(digitalRead(INTERRUPT_SWITCH) == 1)) {
      //setFan(0, OFF);
      digitalWrite(FAN, OFF);
    }
  }


}

//**************************************************************
//****************    MACROS - AUTRES     **********************
//**************************************************************

void getDateAndTime(){
  t = rtc.getTime();
  sunTime[5] = t.year-2000;
  sunTime[4] = t.mon;
  sunTime[3] = t.date;
  sunTime[HEURE] = t.hour;
  sunTime[MINUTE] = t.min;
  sunTime[0] = t.sec;
  myLord.DST(sunTime);
}

//Programme pour convertir l'addition de nombres décimales en format horaire
void convertDecimalToTime(byte * const heure, byte * const minut){
  if ((*minut > 59) && (*minut < 120)){
    *heure += 1;
    *minut = *minut - 60;
  }

  else if ((*minut < 0)&& (minut >= -60)){
    *heure -= 1;
    *minut = 60 - *minut;
  }
}
void convertEEPROMData(){
  rmod = (rmodE - 11);                      //modificateur relais
  vmod = (vmodE - 11);                      //modificateur VENTilation
  f1mod = (f1modE - 11);                    //modificateur fournaise1
  f2mod = (f2modE - 11);                    //modificateur fournaise2
  SRmod = srmod-60;
  SSmod = ssmod-60;
  ROTATION_TIME = (rotation * 1000);       //temps de rotation des moteurs(en mili-secondes)
  PAUSE_TIME = (pause * 1000);             //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
  rampingInterval = (unsigned long)ramping*60*1000;
  P2[HEURE] = HP2;
  P2[MINUTE] = MP2;
}

void setSunriseSunSet(){
  myLord.SunRise(sunTime); ///On détermine l'heure du lever du soleil
  myLord.DST(sunTime);//ajuster l'heure du lever en fonction du changement d'heure
  sunRise[HEURE] = sunTime[HEURE];
  sunRise[MINUTE] = sunTime[MINUTE];

  /* Sunset: */
  myLord.SunSet(sunTime); // Computes Sun Set. Prints:
  myLord.DST(sunTime);
  sunSet[HEURE] = sunTime[HEURE];
  sunSet[MINUTE] = sunTime[MINUTE];
}

void setOutputsTempCible(){
    TEMP_ROLLUP = TEMP_CIBLE + rmod;
    TEMP_VENTILATION = TEMP_CIBLE + vmod;
    TEMP_FOURNAISE1 = TEMP_CIBLE + f1mod;
    TEMP_FOURNAISE2 = TEMP_CIBLE + f2mod;
}

//Programme d'ouverture des rollup
void openSides() {
  if (firstOpening == true){
    incrementCounter = 0;
    firstOpening = false;
  }
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
    Serial.print("incrementCounter:");
    Serial.println(incrementCounter);
    Serial.print("increments:");
    Serial.println(increments);
    lcd.setCursor(0, 1);
    lcd.print(F("ROLLUPS:  "));
    lcd.setCursor(9, 1);
    lcd.print(incrementCounter);
    delay(PAUSE_TIME);
  }

}

//Programme de fermeture des rollups
void closeSides() {
  if (firstOpening == true){
    incrementCounter = 5;
    firstOpening = false;
  }
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


void lcdPrintRollups(){
    lcd.setCursor(0, 1); lcd.print(F("                    "));
    lcd.setCursor(0, 1); lcd.print(F("ROLLUPS:  "));
    lcd.setCursor(9, 1); lcd.print(incrementCounter);
 }
void lcdPrintTemp(){
    lcd.setCursor(0,0); lcd.print(F("                    "));
    lcd.setCursor(0,0); lcd.print(F("T:")); lcd.print(greenhouseTemperature); lcd.print(F("C |TC:"));
    lcd.setCursor(13,0); lcd.print(TEMP_CIBLE); lcd.print(F("C"));
}
void lcdPrintTime(){
    lcd.setCursor(9,3); lcd.print(F("|(P")); lcd.print(PROGRAMME); lcd.print(F(":"));
    lcd.setCursor(14,3); lcdPrintDigits(sunTime[HEURE]); lcd.print(F(":")); lcdPrintDigits(sunTime[MINUTE]);
    lcd.setCursor(19,3); lcd.print(F(")"));
}

void lcdPrintOutputsStatus(){
  lcd.setCursor(0, 2); lcd.print(F("                    "));
  lcd.setCursor(0, 3); lcd.print(F("         "));

  if (cooler[0] == true){
    lcd.setCursor(0, 3); lcd.print(F("FAN:"));
    if (digitalRead(FAN) == OFF) {      lcd.setCursor(5, 3); lcd.print(F("OFF"));    }
    else if (digitalRead(FAN) == ON) {      lcd.setCursor(5, 3); lcd.print(F("ON "));    }
  }

  if (heater[0] == true){
    lcd.setCursor(0, 2); lcd.print(F("H1:"));
    if (digitalRead(CHAUFFAGE1) == OFF) {      lcd.setCursor(5, 2); lcd.print(F("OFF |"));    }
    else if (digitalRead(CHAUFFAGE1) == ON) {      lcd.setCursor(5, 2); lcd.print(F("ON  |"));    }
  }

  if (heater[1] == true){
    lcd.setCursor(11, 2); lcd.print(F("H2:"));
    if (digitalRead(CHAUFFAGE2) == OFF) {      lcd.setCursor(16, 2); lcd.print(F("OFF"));    }
    else if (digitalRead(CHAUFFAGE2) == ON) {      lcd.setCursor(16, 2); lcd.print(F("ON "));    }
  }
}

void lcdPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10)
  lcd.print("0");
  lcd.print(digits);
}

void serialPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10)
  Serial.print("0");
}



//**************************************************************
//****************    MACROS - MENU     ***********************
//**************************************************************

void Menu(int x) {

  if (menuPinState == 1) {
    displayMenu(menu);
    menuPinState = 0;
  }

  int a = analogRead(A0);
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
  else if (x < 150) {
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
  Serial.print(F(",  "));
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

          t = rtc.getTime();
          sunTime[HEURE] = t.hour;
          sunTime[MINUTE] = t.min;
          sunTime[0] = t.sec;
          myLord.DST(sunTime);

          lcd.setCursor(0, 1); lcd.print(F("Time : ")); lcdPrintDigits(sunTime[HEURE]);lcd.print(":"); lcdPrintDigits(sunTime[MINUTE]);lcd.print(":");lcdPrintDigits(sunTime[MINUTE]);
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
      t = rtc.getTime();
      sunTime[HEURE] = t.hour;
      myLord.DST(sunTime);

      if ((t.hour != sunTime[HEURE]) && (y < Nbitems)) {
        hr = y-1;
        switchmenu(131);
      }
      else if ((t.hour == sunTime[HEURE]) && (y < Nbitems)) {
        hr = y;
        switchmenu(131);
      }
      else{
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
          if (fan[1] == false) {
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
          if (heating[1] == false) {
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
          if (heating[1] == false) {
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
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P1[HEURE]); lcd.print(F(":")); lcd.print(P1[MINUTE]); lcd.print(F(" | HF: ")); lcd.print(P2[HEURE]); lcd.print(F(":")); lcd.print(P2[MINUTE]);
          lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLEP1); lcd.print(F("C"));
          lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P2[HEURE]); lcd.print(F(":")); lcd.print(P2[MINUTE]); lcd.print(F(" | HF: ")); lcd.print(P3[HEURE]); lcd.print(F(":")); lcd.print(P3[HEURE]);
          lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(TEMP_CIBLEP2); lcd.print(F("C"));
          lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 3:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P3[HEURE]); lcd.print(F(":")); lcd.print(P3[HEURE]); lcd.print(F(" | HF: ")); lcd.print(P1[HEURE]); lcd.print(F(":")); lcd.print(P1[MINUTE]);
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
    //SET MINUTES PAST/BEFORE SUNRISE
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
        TEMP_CIBLEP1 = y-1;
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
        P2[HEURE] = y;
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
        P2[MINUTE] = (y - 1);
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
        TEMP_CIBLEP2 = y-1;
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
    //SET MINUTES PAST/BEFORE SUNSET
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
        TEMP_CIBLEP3 = y-1;
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
        rampingInterval = ((unsigned long)y-1)*60*1000;
        EEPROM.write(19, ramping);
        switchmenu(5);
      }
      else {
        switchmenu(5);
      }
      break;
  }

}
