#include <Arduino.h>

//*****************LIBRAIRIES************************
#include <EEPROM.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <TimeLord.h>
#include <Greenhouse.h>

//*****************DEFINITIONS***********************
//#define DS18B20
#define SHT1X
#define LCDKEYPAD A0
#define MENU_PIN 2
const byte SAFETY_SWITCH[2] = {2,3};
const byte ROLLUP_OPEN[1] = {4};
const byte ROLLUP_CLOSE[1] = {5};
const byte CHAUFFAGE[2] = {6,8}; // relais fournaise2
const byte FAN[2] = {7,9}; // relais fournaise2

//*********************OBJETS************************
//---------------------LCD-----------------
#define I2C_ADDR    0x27              // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3
LiquidCrystal_I2C  lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7);
//---------------------RTC-----------------
DS3231  rtc(SDA, SCL);                // Init the DS3231 using the hardware interface
Time  t;
//--------------------Sonde-------------
#ifdef DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
  #define ONE_WIRE_BUS A1
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sensors(&oneWire);
#endif
#ifdef SHT1X
  #include <SHT1x.h>
  #define dataPin  A1
  #define clockPin A2
  SHT1x sht1x(dataPin, clockPin);
#endif
//-------------------Timelord-------------
const int TIMEZONE = -5; //PST
const float LATITUDE = 45.50, LONGITUDE = -73.56; // set your position here
TimeLord myLord; // TimeLord Object, Global variable

//*********************VARIABLES GLOBALES*************
//nombre d'items activés
const int nbPrograms = 5;              //Nombre de programmes de température
boolean rollups[1];
boolean heaters[1];
boolean fans[1];
boolean fanSafety[sizeof(fans)];
boolean rollupSafety[sizeof(rollups)];

//Sonde de température
float greenhouseTemperature;     //données de la sonde de température
boolean failedSensor = false;

//Poisition des Rollups
volatile byte incrementCounter[sizeof(rollups)];               //positionnement des rollups
boolean firstOpening[sizeof(rollups)];

//Programmes horaire
byte sunTime[6];                      //données de l'horloge
byte program;                         //Programme en cour
byte lastProgram;                    //Dernier programme en cour

//Programmes de température
float tempCible;

//Ramping
int newTempCible;                   //Température cible à atteindre
unsigned long lastCount = 0;         //Compteur

//Autres variables
const int sleeptime = 1000;           //Temps de pause entre chaque exécution du programme(en mili-secondes)

//-----------------------LCD Menu------------------
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
  #ifdef DS18B20
    sensors.begin();
  #endif
  initLCD(20,4);
  initTimeLord();

  defineProgram(1, SR, 0, -30, 20);
  defineProgram(2, SR, 0, 20, 21);
  defineProgram(3, CLOCK, 11, 10, 22);
  defineProgram(4, SS, 0, -60, 28);
  defineProgram(5, SS, 0, -2, 18);
  defineRollup(1, 5, 1, 1, 0, 1, false);
  defineFan(1, 2, 1, false);
  defineHeater(1, -3, 1);
  defineHeater(2, -5, 1);
  defineRamping(5);

  initVariables();
  initOutputs();
  //Définition des sorties
  initRollupOutput(1, ROLLUP_OPEN[0], ROLLUP_CLOSE[0], LOW);
  initFanOutput(1,FAN[0], LOW);
  initHeaterOutput(1,CHAUFFAGE[0], LOW);
  initHeaterOutput(2,CHAUFFAGE[1], LOW);

  pinMode(LCDKEYPAD, INPUT_PULLUP);
  pinMode(SAFETY_SWITCH[0], INPUT_PULLUP);
  pinMode(SAFETY_SWITCH[1], INPUT_PULLUP);
  pinMode(MENU_PIN, INPUT_PULLUP);
  //Affichage LCD
  lcdDisplay();
}


//**************************************************************
//******************       LOOP      ***************************
//**************************************************************

void loop() {

  //MODE MENU
  if (digitalRead(MENU_PIN) == LOW) {
    //Programme de menu
    Menu(menu);
    delay(50);
  }

  //MODE CONTROLE
  else if (digitalRead(MENU_PIN) == HIGH) {
    //Protocole de controle
    selectProgram();
    //Définition de la température d'activation des sorties
    startRamping();
    //Actualise la température
    getTemperature();
    //Protocoles spéciaux (pré-jour/pré-nuit)
    specialPrograms();
    //Affichage LCD
    lcdDisplay();
    //Activation des relais
    relayLogic();
    //Affichage LCD
    lcdDisplay();
    //Pause entre chaque cycle
    delay(sleeptime);
  }
}


//**************************************************************
//****************    MACROS - SETUP     ***********************
//**************************************************************
void initLCD(byte length, byte width){
  lcd.begin(length, width);
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
}

void initTimeLord(){
  myLord.TimeZone(TIMEZONE * 60);
  myLord.Position(LATITUDE, LONGITUDE);
  myLord.DstRules(3,2,11,1,60); // DST Rules for USA
}

void initOutputs(){
  //Activation des items
  for (int x = 0; x < sizeof(rollups);x++){
    rollups[x] = true;
    firstOpening[x] = true;
    rollupSafety[x] = EEPROM.read(ROLLUPSAFETY+x);
    if (rollupSafety[x] == true) {
        attachInterrupt(digitalPinToInterrupt(SAFETY_SWITCH[0]), endOfTheRun0, FALLING);
        attachInterrupt(digitalPinToInterrupt(SAFETY_SWITCH[1]), endOfTheRun1, FALLING);
    }
  }
  for (int x = 0; x < sizeof(fans);x++){
    fans[x] = true;
    fanSafety[x] = EEPROM.read(FANSAFETY+x);
  }
  for (int x = 0; x < sizeof(heaters);x++){
    heaters[x] = true;
  }
}

void endOfTheRun0(){
  incrementCounter[0] = EEPROM.read(INCREMENTS);
}
void endOfTheRun1(){
  byte x;
  switch(sizeof(rollups)){
    case 1 : x = 0;
    case 2 : x = 1;
  }
  incrementCounter[x] = EEPROM.read(INCREMENTS+x);
}

void initVariables(){
  //Première lecture d'horloge pour définir le lever et coucher du soleil
  getDateAndTime();
  //Définition du lever et du coucher du soleil
  selectProgram();
  //Définition de la température cible
  setTempCible();
  //Définition de la température d'activation des sorties
  getTemperature();
}

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

void selectProgram(){
  //Définition des variables locales
  int P[nbPrograms][3];
  byte sunRise[6];
  byte sunSet[6];
  int srmod[nbPrograms];
  int ssmod[nbPrograms];
  byte programType[nbPrograms];

  for(byte x = 0; x < nbPrograms; x++){
    programType[x] = EEPROM.read(PROGRAMS+x);
    srmod[x] =  EEPROM.read(SRMOD+x)-60;
    ssmod[x] =  EEPROM.read(SSMOD+x)-60;
  }
  //Exécution du programme
  //Définit l'heure du lever et coucher du soleil
  myLord.SunRise(sunTime); ///On détermine l'heure du lever du soleil
  myLord.DST(sunTime);//ajuster l'heure du lever en fonction du changement d'heure
  sunRise[HEURE] = sunTime[HEURE];
  sunRise[MINUTE] = sunTime[MINUTE];
  //Serial.print("lever du soleil :");Serial.print(sunRise[HEURE]);  Serial.print(":");  Serial.println(sunRise[MINUTE]);

  /* Sunset: */
  myLord.SunSet(sunTime); // Computes Sun Set. Prints:
  myLord.DST(sunTime);
  sunSet[HEURE] = sunTime[HEURE];
  sunSet[MINUTE] = sunTime[MINUTE];
  //Serial.print("coucher du soleil :");  Serial.print(sunSet[HEURE]);  Serial.print(":");  Serial.println(sunSet[MINUTE]);
  getDateAndTime();
  //Ajuste l'heure des programmes en fonction du lever et du coucher du soleil
  for(byte x = 0; x < nbPrograms; x++){
    //Serial.println(x);Serial.println (programType[x]);
    if (programType[x] == SR){
      P[x][HEURE] = sunRise[HEURE];
      P[x][MINUTE] = sunRise[MINUTE] + srmod[x];
      convertDecimalToTime(&P[x][HEURE], &P[x][MINUTE]);
      //Serial.print(" Program ");Serial.print(x);Serial.print(" : ");Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);
    }
    else if (programType[x] == CLOCK){
      P[x][HEURE] = PROGRAM_TIME(x, HEURE);
      P[x][MINUTE] = PROGRAM_TIME(x, MINUTE);
      //Serial.print(" Program ");Serial.print(x);Serial.print(" : ");Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);
    }

    else if (programType[x] == SS){
      P[x][HEURE] = sunSet[HEURE];
      P[x][MINUTE] = sunSet[MINUTE] + ssmod[x];
      convertDecimalToTime(&P[x][HEURE], &P[x][MINUTE]);
      //Serial.print(" Program ");Serial.print(x); Serial.print(" : "); Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);

    //Sélectionne le programme en cour
    //Serial.print ("Heure actuelle ");Serial.print(" : ");Serial.print(sunTime[HEURE] );Serial.print(" : ");Serial.println(sunTime[MINUTE]);
    for (byte y = 0; y < (nbPrograms-1); y++){
    //Serial.print ("Programme "); Serial.print(y+1);Serial.print(" : ");Serial.print(P[y][HEURE]);Serial.print(" : ");Serial.println(P[y][MINUTE]);
      if (((sunTime[HEURE] == P[y][HEURE])  && (sunTime[MINUTE] >= P[y][MINUTE]))||((sunTime[HEURE] > P[y][HEURE]) && (sunTime[HEURE] < P[y+1][HEURE]))||((sunTime[HEURE] == P[y+1][HEURE])  && (sunTime[MINUTE] < P[y+1][MINUTE]))){
          program = y+1;
          //Serial.println("YES!");
        }
      }
      //  Serial.print ("Programme ");Serial.print(nbPrograms);Serial.print(" : ");Serial.print(P[nbPrograms-1][HEURE]);Serial.print(" : ");Serial.println(P[nbPrograms-1][MINUTE]);
      if (((sunTime[HEURE] == P[nbPrograms-1][HEURE])  && (sunTime[MINUTE] >= P[nbPrograms-1][MINUTE]))||(sunTime[HEURE] > P[nbPrograms-1][HEURE])||(sunTime[HEURE] < P[0][HEURE])||((sunTime[HEURE] == P[0][HEURE])  && (sunTime[MINUTE] < P[0][MINUTE]))){
        program = nbPrograms;
      //Serial.println("YES!");
      }
    }
  }
}

void setTempCible(){
  for (byte x = 0; x < nbPrograms; x++){
    if(program == x+1){
      tempCible = EEPROM.read(TEMPCIBLE+x);
      //Serial.println(tempCible);
    }
  }
}

void getTemperature(){
  #ifdef DS18B20
    sensors.requestTemperatures();
    greenhouseTemperature = sensors.getTempCByIndex(0);

    if((greenhouseTemperature < -20.00)||(greenhouseTemperature > 80)){
      greenhouseTemperature = EEPROM.read(TEMPCIBLE+program-1)+2;
      failedSensor = true;
    }
    else{
      failedSensor = false;
    }
  #endif
  #ifdef SHT1X
    greenhouseTemperature = sht1x.readTemperatureC();
    if((greenhouseTemperature < -20.00)||(greenhouseTemperature > 80)){
      greenhouseTemperature = EEPROM.read(TEMPCIBLE+program-1)+2;
      failedSensor = true;
    }
    else{
      failedSensor = false;
    }
  #endif

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



//**************************************************************
//****************    MACROS - CONTROLE     ********************
//**************************************************************

//Programme de courbe de température
void startRamping(){
  //Définition des variables locales
  byte tempCibleE[nbPrograms];
  unsigned int RAMPING_INTERVAL = EEPROM.read(RAMPING)*60*1000;
  Serial.println((unsigned long)EEPROM.read(RAMPING)*60*1000);
  for (byte x = 0; x < nbPrograms; x++){
    tempCibleE[x] = EEPROM.read(TEMPCIBLE+x);
  }

  //Exécution du programme
  for (byte x = 0; x < nbPrograms; x++){
    if(program == x+1){
      newTempCible = tempCibleE[x];
    }
  }

  if (newTempCible > tempCible){
    unsigned long rampingCounter = millis();
    Serial.println(rampingCounter);
    Serial.println(rampingCounter - lastCount);
    if(rampingCounter - lastCount > RAMPING_INTERVAL) {
      lastCount = rampingCounter;
      tempCible += 0.5;
    }
  }
  else if (newTempCible < tempCible){
    unsigned long rampingCounter = millis();
    if(rampingCounter - lastCount > RAMPING_INTERVAL) {
      lastCount = rampingCounter;
      tempCible -= 0.5;
    }
  }
}


void specialPrograms(){}

void relayLogic(){                     //Température cible

  float tempRollup[sizeof(rollups)];         //Température d'activation des rollups (max.2)
  float tempHeater[sizeof(heaters)];         //Température d'activation des fournaises (max.2)
  float tempFan[sizeof(fans)];                //Température d'activation des fans (max.2)
  //Définition des variables locales
  byte hystRollups[sizeof(rollups)];
  byte hystHeater[sizeof(heaters)];
  byte hystFan[sizeof(fans)];

  for(byte x = 0; x < sizeof(rollups); x++){
    tempRollup[x] = tempCible + EEPROM.read(RMOD+x) -10;
    hystRollups[x] = EEPROM.read(RHYST+x);
  }
  for(byte x = 0; x < sizeof(heaters); x++){
    tempHeater[x] = tempCible + EEPROM.read(HMOD+x)-10;
    hystHeater[x] = EEPROM.read(HHYST+x);
  }
  for(byte x = 0; x < sizeof(rollups); x++){
    tempFan[x] = tempCible + EEPROM.read(VMOD+x)-10;
    hystFan[x] = EEPROM.read(VHYST+x);
  }

  //Exécution du programme
  lcd.noBlink();
  //Programme d'ouverture/fermeture des rollups
  for(byte x = 0; x < sizeof(rollups); x++){
    if (rollups[x] == true){
      if (greenhouseTemperature < (tempRollup[x] - hystRollups[x])) {
        closeSides(x);
      } else if (greenhouseTemperature > tempRollup[x]) {
        openSides(x);
      }
    }
  }

  //Programme fournaise
  for(byte x = 0; x < sizeof(heaters); x++){
    if (heaters[x] == true){

      if ((greenhouseTemperature < tempHeater[x])&&(incrementCounter[0] == 0)) {
        digitalWrite(CHAUFFAGE[x], ON);
      } else if ((greenhouseTemperature > (tempHeater[x] + hystHeater[x]))||(incrementCounter[0] != 0)) {
        digitalWrite(CHAUFFAGE[x], OFF);
      }
    }
  }

  //Programme ventilation forcée
  for(byte x = 0; x < sizeof(fans); x++){
    if (fans[x] == true){
      if (fanSafety[x] == true){
        if (greenhouseTemperature > tempFan[x]&&(digitalRead(SAFETY_SWITCH[0]) == OFF)){
          digitalWrite(FAN[x], ON);
        }
        else if ((greenhouseTemperature < (tempFan[x] - hystFan[x]))||(digitalRead(SAFETY_SWITCH[0]) == ON)) {
          digitalWrite(FAN[x], OFF);
        }
      }
      else if (fanSafety[x] == false){
        if (greenhouseTemperature > tempFan[x]){
          digitalWrite(FAN[x], ON);
        }
        else if (greenhouseTemperature < (tempFan[x] - hystFan[x])) {
          digitalWrite(FAN[x], OFF);
        }
      }
    }
  }
}

//**************************************************************
//****************    MACROS - AUTRES     **********************
//**************************************************************




//Programme d'ouverture des rollup
void openSides(byte number) {
  unsigned int pause = EEPROM.read(PAUSE+number) * 1000;

  if (firstOpening[number] == true){
    incrementCounter[number] = 0;
    firstOpening[number] = false;
  }
  if (incrementCounter[number] < EEPROM.read(INCREMENTS)) {
  incrementCounter[number] += 1;
    lcd.setCursor(0, 1);
    lcd.print(F("OUVERTURE"));
    digitalWrite(ROLLUP_OPEN[number], ON);
    delay(EEPROM.read(ROTATION+number) * 1000);
    digitalWrite(ROLLUP_OPEN[number], OFF);
    lcd.setCursor(0, 1);
    lcd.print(F("R-U:     "));
    lcd.setCursor(5, 1);
    lcd.print(incrementCounter[number]);
    delay(pause);
  }

}

//Programme de fermeture des rollups
void closeSides(byte number) {
  unsigned int pause = EEPROM.read(PAUSE+number) * 1000;

  if (firstOpening[number] == true){
    incrementCounter[number] = EEPROM.read(INCREMENTS+number);
    firstOpening[number] = false;
  }
  if (incrementCounter[number] > 0) {
    incrementCounter[number] -= 1;
    lcd.setCursor(0, 1);
    lcd.print(F("FERMETURE"));
    digitalWrite(ROLLUP_CLOSE[number], ON);
    delay(EEPROM.read(ROTATION+number) * 1000);
    digitalWrite(ROLLUP_CLOSE[number], OFF);
    lcd.setCursor(0, 1);
    lcd.print(F("R-U:     "));
    lcd.setCursor(5, 1);
    lcd.print(incrementCounter[number]);
    delay(pause);
  }
}


void lcdPrintRollups(){
    lcd.setCursor(0, 1); lcd.print(F("         "));
    lcd.setCursor(0, 1); lcd.print(F("R-U: "));
    lcd.setCursor(5, 1); lcd.print(incrementCounter[0]);
 }
void lcdPrintTemp(){
    lcd.setCursor(0,0); lcd.print(F("                    "));
    if(failedSensor == false){
      lcd.setCursor(0,0); lcd.print(F("T:")); lcd.print(greenhouseTemperature); lcd.print(F("C |TC:"));
    }
    else{
      lcd.setCursor(0,0); lcd.print(F("T:")); lcd.print("!!!"); lcd.print(F("("));lcd.print((int)greenhouseTemperature);lcd.print(F(")|TC:"));
    }
    lcd.setCursor(13,0); lcd.print(tempCible); lcd.print(F("C"));
}
void lcdPrintTime(){
    lcd.setCursor(9,1); lcd.print(F("|(P")); lcd.print(program); lcd.print(F(":"));
    lcd.setCursor(14,1); lcdPrintDigits(sunTime[HEURE]); lcd.print(F(":")); lcdPrintDigits(sunTime[MINUTE]);
    lcd.setCursor(19,1); lcd.print(F(")"));
}

void lcdPrintOutputsStatus(){
  lcd.setCursor(0, 2); lcd.print(F("                    "));
  lcd.setCursor(0, 3); lcd.print(F("                    "));

  if (fans[0] == true){
      lcd.setCursor(0, 3); lcd.print(F("FAN:"));
      if (digitalRead(FAN[0]) == OFF) {      lcd.setCursor(5, 3); lcd.print(F("OFF"));    }
      else if (digitalRead(FAN[0]) == ON) {      lcd.setCursor(5, 3); lcd.print(F("ON "));    }
    }

  if (heaters[0] == true){
    lcd.setCursor(0, 2); lcd.print(F("H1:"));
    if (digitalRead(CHAUFFAGE[0]) == OFF) {      lcd.setCursor(5, 2); lcd.print(F("OFF |"));    }
    else if (digitalRead(CHAUFFAGE[0]) == ON) {      lcd.setCursor(5, 2); lcd.print(F("ON  |"));    }
  }

  if (heaters[1] == true){
    lcd.setCursor(11, 2); lcd.print(F("H2:"));
    if (digitalRead(CHAUFFAGE[1]) == OFF) {      lcd.setCursor(16, 2); lcd.print(F("OFF"));    }
    else if (digitalRead(CHAUFFAGE[1]) == ON) {      lcd.setCursor(16, 2); lcd.print(F("ON "));    }
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
  else if (x < 200) {
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
  for (byte i = 0; i < numLcdRows; ++i) {
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
  for (byte i = 0; i < numLcdRows; ++i) {
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

    case 2: Scrollingmenu(4, "Etat", "setProgram", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 21: Scrollingnumbers((sizeof(rollups)+1), currentMenuItem, 1, 1); break;
    case 22: Scrollingnumbers((sizeof(rollups)+1), currentMenuItem, 1, 1); break;

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
          float temperature;
          #ifdef DS18B20
            sensors.requestTemperatures();
            temperature = sensors.getTempCByIndex(0);
          #endif
          #ifdef SHT1X
              temperature = sht1x.readTemperatureC();
          #endif
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Sonde temp. : ")); lcd.print(temperature); lcd.print(F("C"));
          break;
        case 2: switchmenu(1); break;
        case 3: switchmenu(2); break;
        case 4: switchmenu(3); break;
        case 5: switchmenu(4); break;
        case 6: switchmenu(0); break;
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
    //État, setProgram
    case 2 :
      switch (y) {
        case 1:
          switchmenu(21); break;
        case 2:
          switchmenu(22); break;
        case 3: switchmenu(23); break;
        case 4: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu21-----------------------------------------
    //ETAT DES ROLLUPS
    case 21 :
      if (y < Nbitems) {
        switch(y){
          case 1 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Increment: ")); lcd.print(incrementCounter[0]); lcd.print(F(""));
          lcd.setCursor(0, 2); lcd.print(F("TP: ")); lcd.print(EEPROM.read(PAUSE)); lcd.print(F("s")); lcd.setCursor(8,2);lcd.print(F("| TR: ")); lcd.print(EEPROM.read(ROTATION)); lcd.print(F("s"));
          lcd.setCursor(0, 3); lcd.print(F("H: ")); lcd.print(EEPROM.read(RHYST)); lcd.print(F("C")); lcd.setCursor(8,3);lcd.print(F("| TA: ")); lcd.print(tempCible + byteToNegative(EEPROM.read(RMOD), 10));lcd.print("C");
          break;
          case 2 :
          lcd.setCursor(0, 1); lcd.print(F("Increment: ")); lcd.print(incrementCounter[1]); lcd.print(F("%"));
          lcd.setCursor(0, 2); lcd.print(F("TP: ")); lcd.print(EEPROM.read(PAUSE+1)); lcd.print(F("s")); lcd.setCursor(8,2);lcd.print(F("| TR:")); lcd.print(EEPROM.read(ROTATION+1)); lcd.print(F("s"));
          lcd.setCursor(0, 3); lcd.print(F("H: ")); lcd.print(EEPROM.read(RHYST+1)); lcd.print(F("C")); lcd.setCursor(8,3);lcd.print(F("| TA: ")); lcd.print(tempCible - byteToNegative(EEPROM.read(RMOD+1), 10));lcd.print("C");
          break;
        }
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu22-----------------------------------------
    //SET ROTATION TIME
    case 22 :
      if (y < Nbitems) {
        //rotation = y;
        EEPROM.update(ROTATION, y);
        switchmenu(2);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu23-----------------------------------------
    //"5", "15", "20", "30", "45", "60", "75", "90", "120", "back"
    case 23 :
      int pause;
      switch (y) {
        case 1: pause = 5; switchmenu(2); EEPROM.write(PAUSE, pause);break;
        case 2: pause = 15; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 3: pause = 20; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 4: pause = 30; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 5: pause = 45; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 6: pause = 60; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 7: pause = 75; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 8: pause = 90; switchmenu(2); EEPROM.write(PAUSE, pause); break;
        case 9: pause = 120; switchmenu(2); EEPROM.write(PAUSE, pause); break;
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
          if (digitalRead(FAN[0]) == OFF) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(tempCible+EEPROM.read(VMOD)-10); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(EEPROM.read(VHYST)); lcd.print(F("C"));
          break;
        case 2: switchmenu(31); break;
        case 3: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu31-----------------------------------------
    //SET HYSTERESIS
    case 31 :
      if (y < Nbitems) {
        switchmenu(3);
        EEPROM.write(1, y);
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
          if (digitalRead(CHAUFFAGE[0]) == OFF) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(tempCible+EEPROM.read(HMOD)-10); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(EEPROM.read(HHYST)); lcd.print(F("C"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("FOURNAISE(2) : "));
          if (digitalRead(CHAUFFAGE[1]) == OFF) {
            lcd.print(F("OFF"));
          }
          else {
            lcd.print(F("ON"));
          }
          lcd.setCursor(0, 2); lcd.print(F("Temp. cible : ")); lcd.print(tempCible+EEPROM.read(HMOD+1)-10); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("Hysteresis : ")); lcd.print(EEPROM.read(HHYST+1)); lcd.print(F("C"));
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
        switchmenu(4);
        EEPROM.update(HHYST, y);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu42-----------------------------------------
    //SET HYSTERESIS
    case 42 :
      if (y < Nbitems) {
        switchmenu(4);
        EEPROM.write(HHYST+1, y);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu5-----------------------------------------
    //"Programme 1", "Programme 2", "Programme 3", "Modificateurs" "Set Programme 1", "Set Programme 2", "Set Programme 3", "Set Modificateurs", "back"
    case 5 :/*
      switch (y) {
        case 1:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P[0][HEURE]); lcd.print(F(":")); lcd.print(P[0][MINUTE]); lcd.print(F(" | HF: ")); lcd.print(P[1][HEURE]); lcd.print(F(":")); lcd.print(P[1][MINUTE]);
          //lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(tempCibleE[0]); lcd.print(F("C"));
          //lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(ramping); lcd.print(F(" min"));
          break;
        case 2:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P[1][HEURE]); lcd.print(F(":")); lcd.print(P[1][MINUTE]); lcd.print(F(" | HF: ")); lcd.print(P[2][HEURE]); lcd.print(F(":")); lcd.print(P[2][HEURE]);
          //lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(tempCibleE[1]); lcd.print(F("C"));
          //lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(RAMPING); lcd.print(F(" min"));
          break;
        case 3:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("HD: ")); lcd.print(P[2][HEURE]); lcd.print(F(":")); lcd.print(P[2][HEURE]); lcd.print(F(" | HF: ")); lcd.print(P[0][HEURE]); lcd.print(F(":")); lcd.print(P[0][MINUTE]);
          //lcd.setCursor(0, 2); lcd.print(F("TEMP.CIBLE : ")); lcd.print(tempCibleE[2]); lcd.print(F("C"));
          //lcd.setCursor(0,3); lcd.print(F("RAMPING : "));  lcd.print(RAMPING); lcd.print(F(" min"));
          break;
        case 4:
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("TEMP.CIBLE : ")); lcd.print(tempCible);
          lcd.setCursor(0, 2); lcd.print(F("RMod:")); lcd.print(EEPROM.read(RMOD)-11);
          lcd.setCursor(8, 2); lcd.print(F("| HMOD[0]:")); lcd.print(EEPROM.read(HMOD)-11);
          lcd.setCursor(0, 3); lcd.print(F("VMOD[0]:")); lcd.print(EEPROM.read(VMOD)-11);
          lcd.setCursor(8, 3); lcd.print(F("| HMOD[1]:")); lcd.print(EEPROM.read(HMOD+1)-11);
          break;
        case 5: switchmenu(51); break;
        case 6: switchmenu(52); break;
        case 7: switchmenu(53); break;
        case 8: switchmenu(54); break;
        case 9: switchmenu(55); break;
        case 10: switchmenu(0); break;
      }*/
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
        EEPROM.write(SSMOD, y);
        switchmenu(51);
      }
      else {
        switchmenu(51);
      }
      break;
    //-------------------------------SelectMenu512-----------------------------------------
    //SET tempCible
    case 512 :
      if (y < Nbitems) {
        //tempCibleE[0] = y-1;
        switchmenu(51);
        //EEPROM.write(TEMP_CIBLEE, y-1);
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
        //P[1][HEURE] = y;
        switchmenu(5211);
        //EEPROM.write(7, P[1][HEURE]);
      }
      else {
        switchmenu(52);
      }
      break;
    //-------------------------------SelectMenu5211-----------------------------------------
    //SET MINUTES
    case 5211 :
      if (y < Nbitems) {
        //P[1][MINUTE] = (y - 1);
        switchmenu(52);
        //EEPROM.write(8, P[1][MINUTE]);
      }
      else {
        switchmenu(52);
      }
      break;
    //-------------------------------SelectMenu522-----------------------------------------
    //SET tempCible
    case 522 :
      if (y < Nbitems) {
        //tempCibleE[1] = y-1;
        switchmenu(52);
        //EEPROM.write(9, tempCibleE[1]);
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
        //SSMOD[0] = y;
        //EEPROM.write(10, SSMOD[0]);
        switchmenu(53);
      }
      else {
        switchmenu(53);
      }
      break;
    //-------------------------------SelectMenu532-----------------------------------------
    //SET tempCible
    case 532 :
      if (y < Nbitems) {
        //tempCibleE[2] = y-1;
        switchmenu(53);
        //EEPROM.write(12, tempCibleE[2]);
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
        //tempRollup = tempCible + y -11;
        switchmenu(54);
        EEPROM.write(15, y);
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 542 :
      if (y < Nbitems) {
        switchmenu(54);
        EEPROM.write(16, y);
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 543 :
      if (y < Nbitems) {
        switchmenu(54);
        EEPROM.write(17, y);
      }
      else {
        switchmenu(54);
      }
      break;
    //-------------------------------SelectMenu541-----------------------------------------
    //SET MOD
    case 544 :
      if (y < Nbitems) {
        switchmenu(54);
        EEPROM.write(18, y);
      }
      else {
        switchmenu(54);
      }
      break;//-------------------------------SelectMenu541-----------------------------------------
    //SET ramping interval
    case 55:/*
    if (y < Nbitems) {
        RAMPING = y-1;
        EEPROM.read(RAMPING)*60*1000 = ((unsigned long)y-1)*60*1000;
        EEPROM.write(19, RAMPING);
        switchmenu(5);
      }
      else {
        switchmenu(5);
      }*/
      break;
  }

}
