#include <Arduino.h>

//*****************LIBRAIRIES************************
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <TimeLord.h>
#include <Greenhouse.h>

//*****************DEFINITIONS***********************
//Select your temperature probe
#define DS18B20
//define SHT1X

//Select your ouput pins
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
byte nr; byte inc; unsigned int rot; unsigned int p; int mod; byte hys; boolean sfty;
byte typ; byte tt;
byte timeWithoutDST;

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

  //defineProgram(1, SR, 0, -30, 20);
  //defineProgram(2, SR, 0, 20, 21);
  //defineProgram(3, CLOCK, 11, 10, 22);
  //defineProgram(4, SS, 0, -60, 28);
  //defineProgram(5, SS, 0, -2, 18);

  //defineRollup(1, 5, 1, 1, 0, 1, false);
  //defineFan(1, 2, 1, false);
  //defineHeater(1, -3, 1);
  //defineHeater(2, -5, 1);
  //defineRamping(5);

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
    //Protocoles spéciaux (pré-jour/pré-nuit) [VIDE]
    specialPrograms();
    //Affichage LCD
    lcdDisplay();
    //Activation des relais
    relayLogic();
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
    rollupSafety[x] = rollupSafetyE(x);
    if (rollupSafety[x] == true) {
        attachInterrupt(digitalPinToInterrupt(SAFETY_SWITCH[0]), endOfTheRun0, FALLING);
        attachInterrupt(digitalPinToInterrupt(SAFETY_SWITCH[1]), endOfTheRun1, FALLING);
    }
  }
  for (int x = 0; x < sizeof(fans);x++){
    fans[x] = true;
    fanSafety[x] = fanSafetyE(x);
  }
  for (int x = 0; x < sizeof(heaters);x++){
    heaters[x] = true;
  }
}

void endOfTheRun0(){
  incrementCounter[0] = incrementsE(0);
}
void endOfTheRun1(){
  byte x;
  switch(sizeof(rollups)){
    case 1 : x = 0;
    case 2 : x = 1;
  }
  incrementCounter[x] = incrementsE(x);
}

void initVariables(){
  //Première lecture d'horloge pour définir le lever et coucher du soleil
  getDateAndTime();
  //Définition du lever et du coucher du soleil
  selectProgram();
  //Définition de la température cible
  setTempCible();
  //Définition de la température d'activation des sorties
  //getTemperature();
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
  byte sunRise[3];
  byte sunSet[3];

  //Exécution du programme
  //Définit l'heure du lever et coucher du soleil
  myLord.SunRise(sunTime); ///On détermine l'heure du lever du soleil
  myLord.DST(sunTime);//ajuster l'heure du lever en fonction du changement d'heure
  sunRise[HEURE] = sunTime[HEURE];
  sunRise[MINUTE] = sunTime[MINUTE];
  Serial.print("lever du soleil :");Serial.print(sunRise[HEURE]);  Serial.print(":");  Serial.println(sunRise[MINUTE]);

  /* Sunset: */
  myLord.SunSet(sunTime); // Computes Sun Set. Prints:
  myLord.DST(sunTime);
  sunSet[HEURE] = sunTime[HEURE];
  sunSet[MINUTE] = sunTime[MINUTE];
  Serial.print("coucher du soleil :");  Serial.print(sunSet[HEURE]);  Serial.print(":");  Serial.println(sunSet[MINUTE]);
  getDateAndTime();
  //Ajuste l'heure des programmes en fonction du lever et du coucher du soleil
  for(byte x = 0; x < nbPrograms; x++){
    Serial.println (programsE(x));
    if (programsE(x) == SR){
      P[x][HEURE] = sunRise[HEURE];
      P[x][MINUTE] = sunRise[MINUTE] + srmodE(x);
      convertDecimalToTime(&P[x][HEURE], &P[x][MINUTE]);
      //Serial.print(" Program ");Serial.print(x);Serial.print(" : ");Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);
    }
    else if (programsE(x) == CLOCK){
      P[x][HEURE] = PROGRAM_TIME(x, HEURE);
      P[x][MINUTE] = PROGRAM_TIME(x, MINUTE);
      //Serial.print(" Program ");Serial.print(x);Serial.print(" : ");Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);
    }

    else if (programsE(x) == SS){
      P[x][HEURE] = sunSet[HEURE];
      P[x][MINUTE] = sunSet[MINUTE] + ssmodE(x);
      convertDecimalToTime(&P[x][HEURE], &P[x][MINUTE]);
      //Serial.print(" Program ");Serial.print(x); Serial.print(" : "); Serial.print(P[x][HEURE]);Serial.print(" : ");Serial.println(P[x][MINUTE]);
    }
  }
    //Sélectionne le programme en cour
    //Serial.print ("Heure actuelle ");Serial.print(" : ");Serial.print(sunTime[HEURE] );Serial.print(" : ");Serial.println(sunTime[MINUTE]);
    for (byte y = 0; y < (nbPrograms-1); y++){
    Serial.print ("Programme "); Serial.print(y+1);Serial.print(" : ");Serial.print(P[y][HEURE]);Serial.print(" : ");Serial.println(P[y][MINUTE]);
      if (((sunTime[HEURE] == P[y][HEURE])  && (sunTime[MINUTE] >= P[y][MINUTE]))||((sunTime[HEURE] > P[y][HEURE]) && (sunTime[HEURE] < P[y+1][HEURE]))||((sunTime[HEURE] == P[y+1][HEURE])  && (sunTime[MINUTE] < P[y+1][MINUTE]))){
          program = y+1;
          Serial.println("YES!");
        }
      }
        Serial.print ("Programme ");Serial.print(nbPrograms);Serial.print(" : ");Serial.print(P[nbPrograms-1][HEURE]);Serial.print(" : ");Serial.println(P[nbPrograms-1][MINUTE]);
      if (((sunTime[HEURE] == P[nbPrograms-1][HEURE])  && (sunTime[MINUTE] >= P[nbPrograms-1][MINUTE]))||(sunTime[HEURE] > P[nbPrograms-1][HEURE])||(sunTime[HEURE] < P[0][HEURE])||((sunTime[HEURE] == P[0][HEURE])  && (sunTime[MINUTE] < P[0][MINUTE]))){
        program = nbPrograms;
      Serial.println("YES!");
    }
}



void setTempCible(){
  for (byte x = 0; x < nbPrograms; x++){
    if(program == x+1){
      tempCible = targetTempE(x);
      //Serial.println(tempCible);
    }
  }
}


float greenhouseTemperature() {
  float temp;
  #ifdef DS18B20
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    if((temp < -20.00)||(temp > 80)){
      temp = targetTempE(program-1)+2;
      return temp;
      failedSensor = true;
    }
    else{
      return temp;
      failedSensor = false;
    }
  #endif
  #ifdef SHT1X
    temp = sht1x.readTemperatureC();
    if((temp < -20.00)||(temp > 80)){
      temp = targetTempE(program-1)+2;
      return temp;
      failedSensor = true;
    }
    else{
      return temp;
      failedSensor = false;
    }
  #endif

}

void lcdDisplay() {
  lcd.noBlink();
  //Si passe du mode menu au mode controle...
  if (menuPinState == 0) {
    lcdPrintTemp();
    lcdPrintTempCible();
    lcdPrintTime();
    lcdPrintRollups();
    lcdPrintOutputsStatus();
    menuPinState = 1;
  }
  //Séquence normale...
  lcdPrintTemp();
  lcdPrintTempCible();
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
  Serial.println(rampingE());

  //Exécution du programme
  for (byte x = 0; x < nbPrograms; x++){
    if(program == x+1){
      newTempCible = targetTempE(x);
    }
  }

  if (newTempCible > tempCible){
    unsigned long rampingCounter = millis();
    Serial.println(rampingCounter);
    Serial.println(rampingCounter - lastCount);
    if(rampingCounter - lastCount > rampingE()) {
      lastCount = rampingCounter;
      tempCible += 0.5;
    }
  }
  else if (newTempCible < tempCible){
    unsigned long rampingCounter = millis();
    if(rampingCounter - lastCount > rampingE()) {
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

  for(byte x = 0; x < sizeof(rollups); x++){
    tempRollup[x] = tempCible + rmodE(x);
  }
  for(byte x = 0; x < sizeof(heaters); x++){
    tempHeater[x] = tempCible + hmodE(x);
  }
  for(byte x = 0; x < sizeof(rollups); x++){
    tempFan[x] = tempCible + vmodE(x);
  }

  //Exécution du programme
  lcd.noBlink();
  //Programme d'ouverture/fermeture des rollups
  for(byte x = 0; x < sizeof(rollups); x++){
    if (rollups[x] == true){
      if (greenhouseTemperature() < (tempRollup[x] - rhystE(x))) {
        closeSides(x);
      } else if (greenhouseTemperature() > tempRollup[x]) {
        openSides(x);
      }
    }
  }

  //Programme fournaise
  for(byte x = 0; x < sizeof(heaters); x++){
    if (heaters[x] == true){

      if ((greenhouseTemperature() < tempHeater[x])&&(incrementCounter[0] == 0)) {
        digitalWrite(CHAUFFAGE[x], ON);
      } else if ((greenhouseTemperature() > (tempHeater[x] + hhystE(x)))||(incrementCounter[0] != 0)) {
        digitalWrite(CHAUFFAGE[x], OFF);
      }
    }
  }

  //Programme ventilation forcée
  for(byte x = 0; x < sizeof(fans); x++){
    if (fans[x] == true){
      if (fanSafety[x] == true){
        if (greenhouseTemperature() > tempFan[x]&&(digitalRead(SAFETY_SWITCH[0]) == OFF)){
          digitalWrite(FAN[x], ON);
        }
        else if ((greenhouseTemperature() < (tempFan[x] - vhystE(x)))||(digitalRead(SAFETY_SWITCH[0]) == ON)) {
          digitalWrite(FAN[x], OFF);
        }
      }
      else if (fanSafety[x] == false){
        if (greenhouseTemperature() > tempFan[x]){
          digitalWrite(FAN[x], ON);
        }
        else if (greenhouseTemperature() < (tempFan[x] - vhystE(x))) {
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
  unsigned int pause = pauseE(number);

  if (firstOpening[number] == true){
    incrementCounter[number] = 0;
    firstOpening[number] = false;
  }
  if (incrementCounter[number] < incrementsE(number)) {
  incrementCounter[number] += 1;
    lcd.setCursor(0, 1);
    lcd.print(F("OUVERTURE"));
    digitalWrite(ROLLUP_OPEN[number], ON);
    delay(rotationE(number));
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
  unsigned int pause = pauseE(number);

  if (firstOpening[number] == true){
    incrementCounter[number] = incrementsE(number);
    firstOpening[number] = false;
  }
  if (incrementCounter[number] > 0) {
    incrementCounter[number] -= 1;
    lcd.setCursor(0, 1);
    lcd.print(F("FERMETURE"));
    digitalWrite(ROLLUP_CLOSE[number], ON);
    delay(rotationE(number));
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
    lcd.setCursor(0,0); lcd.print(F("         "));
    if(failedSensor == false){
      lcd.setCursor(0,0); lcd.print(F("T:")); lcd.print(greenhouseTemperature()); lcd.print(F("C"));
    }
    else{
      lcd.setCursor(0,0); lcd.print(F("T:")); lcd.print("!!!"); lcd.print(F("("));lcd.print((int)greenhouseTemperature());lcd.print(F(")"));
    }
}
void lcdPrintTempCible(){
    lcd.setCursor(9,0);lcd.print("|TC:");lcd.print(tempCible); lcd.print(F("C"));
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
      if (digitalRead(FAN[0]) == OFF) {lcd.setCursor(5, 3); lcd.print(F("OFF"));    }
      else if (digitalRead(FAN[0]) == ON) {lcd.setCursor(5, 3); lcd.print(F("ON "));    }
    }

  if (heaters[0] == true){
    lcd.setCursor(0, 2); lcd.print(F("H1:"));
    if (digitalRead(CHAUFFAGE[0]) == OFF) {lcd.setCursor(5, 2); lcd.print(F("OFF |"));    }
    else if (digitalRead(CHAUFFAGE[0]) == ON) {lcd.setCursor(5, 2); lcd.print(F("ON  |"));    }
  }

  if (heaters[1] == true){
    lcd.setCursor(11, 2); lcd.print(F("H2:"));
    if (digitalRead(CHAUFFAGE[1]) == OFF) {lcd.setCursor(16, 2); lcd.print(F("OFF"));    }
    else if (digitalRead(CHAUFFAGE[1]) == ON) {lcd.setCursor(16, 2); lcd.print(F("ON "));    }
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

void Scrollingmenu (const char t[6] PROGMEM, int x, const char a[20] PROGMEM, const char b[20] PROGMEM, const char c[20] PROGMEM, const char d[20] PROGMEM, const char e[20] PROGMEM, const char f[20] PROGMEM, const char g[20] PROGMEM, const char h[20] PROGMEM, const char i[20] PROGMEM, const char j[20] PROGMEM, int y) {
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
  lcd.setCursor(14,0);lcd.print(t);
  for (byte i = 0; i < numLcdRows; ++i) {
    lcd.setCursor(0, i + 1);
    lcd.print(menuitems[i + scrollPos]);
  }
  lcd.setCursor(19, (y - scrollPos));
  lcd.blink();
}

void Scrollingnumbers(const char t[6] PROGMEM,int x, int y, int z, int a) {
  const int numLcdRows = 3;
  int scrollPos = 0;
  Nbitems = x;

  if (y > numLcdRows) {
    scrollPos = y - numLcdRows;
  } else {
    scrollPos = 0;
  }
  clearPrintTitle();
  lcd.setCursor(14,0);lcd.print(t);
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
    case 0: Scrollingmenu("", 6, "Capteurs", "Date/time", "Rollups", "Ventilation", "Chauffage", "Programmes", "", "", "", "", currentMenuItem); break;
    case 1: Scrollingmenu("TIME*", 6, "Date", "Time", "SetDOW", "Set date", "Set time", "back", "", "", "", "", currentMenuItem); break;

    case 11: Scrollingmenu("DOW**", 8, "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "back", "", "", currentMenuItem); break;
    case 12: Scrollingnumbers("YEAR*",11, currentMenuItem, 2016, 1);  break;
    case 121: Scrollingnumbers("MONTH", 13, currentMenuItem, 1 , 1);  break;
    case 1211: Scrollingnumbers("DAY**", 32, currentMenuItem, 1, 1); break;
    case 13: Scrollingnumbers("HOUR*",25, currentMenuItem, 1, 1); break;
    case 131: Scrollingnumbers("MIN***",62, currentMenuItem, 0 , 1); break;
    case 1311: Scrollingnumbers("SEC**",62, currentMenuItem, 0, 1); break;

    case 2: Scrollingmenu("ROLL*", 3, "Parameters", "setParameters", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 21: Scrollingmenu("ITEM*",(sizeof(rollups)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 22: Scrollingmenu("ITEM*",(sizeof(rollups)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 221: Scrollingnumbers("INCR*",11, currentMenuItem, 1, 1); break;
    case 2211: Scrollingnumbers("ROTA*", 61, currentMenuItem, 2, 2); break;
    case 22111: Scrollingmenu("PAUSE", 10, "5", "15", "20", "30", "45", "60", "75", "90", "120", "back", currentMenuItem); break;
    case 221111: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 2211111: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;
    case 22111111: Scrollingmenu("SAFTY", 3, "Oui", "Non", "back", "", "", "", "",  "", "", "", currentMenuItem); break;

    case 3: Scrollingmenu("FANS*", 3, "Parameters", "setParameters", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 31: Scrollingmenu("ITEM*",(sizeof(fans)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 32: Scrollingmenu("ITEM*",(sizeof(fans)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 321: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 3211: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;
    case 32111: Scrollingmenu("SAFTY", 3, "Oui", "Non", "back", "", "", "", "",  "", "", "", currentMenuItem); break;

    case 4: Scrollingmenu("HEATR", 3, "Parameters", "setParameters", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 41: Scrollingmenu("ITEM*",(sizeof(heaters)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem);break;
    case 42: Scrollingmenu("ITEM*",(sizeof(heaters)+1),"1", "2", "back", "", "", "", "",  "", "", "", currentMenuItem);break;
    case 421: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 4211: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;

    case 5: Scrollingmenu("PROG*", 3, "Timezones", "setTimezones", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 51: Scrollingnumbers("TIMZ*",(nbPrograms+1),currentMenuItem, 1, 1);break;
    case 52: Scrollingnumbers("TIMZ*",(nbPrograms+1),currentMenuItem, 1, 1);break;
    case 521: Scrollingmenu("ITEM*", 3,"SR", "MANUAL", "SS", "", "", "", "",  "", "", "", currentMenuItem);break;
    case 5211: Scrollingnumbers("HOUR*",25, currentMenuItem, 1, 1); break;
    case 52111: Scrollingnumbers("MIN***",62, currentMenuItem, 0 , 1); break;
    case 5212: Scrollingnumbers("MOD***",122, currentMenuItem, -60 , 1); break;
    case 521111: Scrollingnumbers("TEMP", 42, currentMenuItem, 0, 1);break;

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
          lcd.setCursor(0, 1); lcd.print(F("Sonde temp. : ")); lcd.print(greenhouseTemperature()); lcd.print(F("C"));
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
          getDateAndTime();
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
        case 3: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu21-----------------------------------------
    //PROGRAMMES DES ROLLUPS
    case 21 :
      if (y < Nbitems) {
        switch(y){
          case 1 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("Increments: ")); lcd.print(incrementsE(0)); lcd.print(F(""));
          lcd.setCursor(0, 2); lcd.print(F("TP: ")); lcd.print(pauseE(0)/1000); lcd.print(F("s")); lcd.setCursor(8,2);lcd.print(F("| TR: ")); lcd.print(rotationE(0)/1000); lcd.print(F("s"));
          lcd.setCursor(0, 3); lcd.print(F("H: ")); lcd.print(rhystE(0)); lcd.print(F("C")); lcd.setCursor(8,3);lcd.print(F("| TA: ")); lcd.print(tempCible + rmodE(0));lcd.print("C");
          break;
          case 2 :
          lcd.setCursor(0, 1); lcd.print(F("Increment: ")); lcd.print(incrementsE(1)); lcd.print(F("%"));
          lcd.setCursor(0, 2); lcd.print(F("TP: ")); lcd.print(pauseE(1)/1000); lcd.print(F("s")); lcd.setCursor(8,2);lcd.print(F("| TR:")); lcd.print(rotationE(1)/1000); lcd.print(F("s"));
          lcd.setCursor(0, 3); lcd.print(F("H: ")); lcd.print(rhystE(1)); lcd.print(F("C")); lcd.setCursor(8,3);lcd.print(F("| TA: ")); lcd.print(tempCible - rmodE(1));lcd.print("C");
          break;
        }
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu22-----------------------------------------
    //SET ROLLUP PROGRAM
    case 22 :
      if (y < Nbitems) {
        nr = y;
        switchmenu(221);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu221-----------------------------------------
    //SET INCREMENTS
    case 221 :
      if (y < Nbitems) {
        inc = y;
        switchmenu(2211);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu2211-----------------------------------------
    //SET ROTATION TIME
    case 2211 :
      if (y < Nbitems) {
        rot = y*2;
        switchmenu(22111);
      }
      else {
        switchmenu(2);
      }
      break;
      //-------------------------------SelectMenu22111-----------------------------------------
      //SET PAUSE TIME
      case 22111 :
        switch (y) {
          case 1: p = 5; switchmenu(221111);break;
          case 2: p = 15; switchmenu(221111); break;
          case 3: p = 20; switchmenu(221111); break;
          case 4: p = 30; switchmenu(221111); break;
          case 5: p = 45; switchmenu(221111); break;
          case 6: p = 60; switchmenu(221111); break;
          case 7: p = 75; switchmenu(221111); break;
          case 8: p = 90; switchmenu(221111); break;
          case 9: p = 120; switchmenu(221111); break;
          case 10: switchmenu(2); break;
          }
        break;
      //-------------------------------SelectMenu221111-----------------------------------------
      //SET MODIFICATOR
      case 221111 :
        if (y < Nbitems) {
          mod = y-11;
          switchmenu(2211111);
        }
        else {
          switchmenu(2);
        }
      break;
      //-------------------------------SelectMenu221111-----------------------------------------
      //SET HYSTERESIS
      case 2211111 :
        if (y < Nbitems) {
          hys = y;
          switchmenu(22111111);
        }
        else {
          switchmenu(2);
        }
      break;
      //-------------------------------SelectMenu221111-----------------------------------------
      //SET SAFETY AND UPDATE EEPROM
      case 22111111 :
        switch (y){
          case 1: sfty = true;defineRollup(nr,inc,rot,p,mod,hys,sfty); switchmenu(2);break;
          case 2: sfty = false;defineRollup(nr,inc,rot,p,mod,hys,sfty); switchmenu(2);break;
          case 3: switchmenu(2);break;
        }
      break;
    //-------------------------------SelectMenu31-----------------------------------------
  /*  case 3: Scrollingmenu("FANS*", 3, "Program", "setProgram", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 31: Scrollingnumbers("#****",(sizeof(fans)+1), currentMenuItem, 1, 1); break;
    case 32: Scrollingnumbers("#****", (sizeof(fans)+1), currentMenuItem, 1, 1); break;
    case 321: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 3211: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;
    case 32111: Scrollingmenu("SAFTY", 3, "Oui", "Non", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
*/

//-------------------------------SelectMenu3-----------------------------------------
//État, setProgram
case 3 :
  switch (y) {
    case 1:
      switchmenu(31); break;
    case 2:
      if (sizeof(rollups) == 2){switchmenu(32);}
      else{switchmenu(0);}
      break;
    case 3: switchmenu(0); break;
  }
break;

//-------------------------------SelectMenu31-----------------------------------------
    //PROGRAMME DES FANS
    case 31 :
      if (y < Nbitems) {
        switch(y){
          case 1 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("TA: ")); lcd.print(tempCible + vmodE(0)); lcd.print("C");
          lcd.setCursor(0, 2); lcd.print(F("H: ")); lcd.print(rhystE(0)); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("SAFETY: ")); if(fanSafetyE(0)== true){lcd.print(F("Activated"));}else{lcd.print(F("Deactivated"));}
          break;
          case 2 :
          lcd.setCursor(0, 1); lcd.print(F("TA: ")); lcd.print(tempCible + vmodE(1)); lcd.print("C");
          lcd.setCursor(0, 2); lcd.print(F("H: ")); lcd.print(rhystE(1)); lcd.print(F("C"));
          lcd.setCursor(0, 3); lcd.print(F("SAFETY: ")); if(fanSafetyE(1)== true){lcd.print(F("Activated"));}else{lcd.print(F("Deactivated"));}
          break;
        }
      }
      else {
        switchmenu(3);
      }
      break;
    //-------------------------------SelectMenu32-----------------------------------------
    //SET FAN PROGRAM
    case 32 :
      if (y < Nbitems) {
        nr = y;
        switchmenu(321);
      }
      else {
        switchmenu(3);
      }
      break;
    //-------------------------------SelectMenu321-----------------------------------------
    //SET MOD
    case 321 :
      if (y < Nbitems) {
        mod = y-11;
        switchmenu(3211);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu3211-----------------------------------------
    //SET HYSTERESIS
    case 3211 :
      if (y < Nbitems) {
        hys = y;
        switchmenu(32111);
      }
      else {
        switchmenu(2);
      }
      break;
      //-------------------------------SelectMenu321111-----------------------------------------
      //SET SAFETY AND UPDATE EEPROM
      case 32111 :
        switch (y){
          case 1: sfty = true;defineFan(nr,mod,hys,sfty); switchmenu(3);break;
          case 2: sfty = false;defineFan(nr,mod,hys,sfty); switchmenu(3);break;
          case 3: switchmenu(3);break;
        }
      break;

/*  case 4: Scrollingmenu("HEATR", 3, "Program", "setProgram", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 41: Scrollingnumbers("ITEM*",(sizeof(fans)+1), currentMenuItem, 1, 1); break;
    case 42: Scrollingnumbers("ITEM*", (sizeof(fans)+1), currentMenuItem, 1, 1); break;
    case 421: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 4211: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;
*/

//-------------------------------SelectMenu4-----------------------------------------
//État, setProgram
case 4 :
  switch (y) {
    case 1:
      switchmenu(41); break;
    case 2:
      if (sizeof(fans) == 2){switchmenu(42);}
      else{switchmenu(0);}
      break;
    case 3: switchmenu(0); break;
  }
break;

//-------------------------------SelectMenu41-----------------------------------------
    //PROGRAMME DES CHAUFFERETTES
    case 41 :
      if (y < Nbitems) {
        switch(y){
          case 1 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1); lcd.print(F("TA: ")); lcd.print(tempCible + hmodE(0)); lcd.print("C");
          lcd.setCursor(0, 2); lcd.print(F("H: ")); lcd.print(hhystE(0)); lcd.print(F("C"));
          break;
          case 2 :
          lcd.setCursor(0, 1); lcd.print(F("TA: ")); lcd.print(tempCible + hmodE(1)); lcd.print("C");
          lcd.setCursor(0, 2); lcd.print(F("H: ")); lcd.print(hhystE(1)); lcd.print(F("C"));
          break;
        }
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu42-----------------------------------------
    //SET CHAUFFERETTES PROGRAM
    case 42 :
      if (y < Nbitems) {
        nr = y;
        switchmenu(421);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu421-----------------------------------------
    //SET MOD
    case 421 :
      if (y < Nbitems) {
        mod = y-11;
        switchmenu(4211);
      }
      else {
        switchmenu(4);
      }
      break;
    //-------------------------------SelectMenu4211-----------------------------------------
    //SET HYSTERESIS
    case 4211 :
      if (y < Nbitems) {
        hys = y;
        defineHeater(nr,mod,hys);
        switchmenu(4);
      }
      else {
        switchmenu(4);
      }
      break;
      /*
      case 5: Scrollingmenu("PROG*", 3, "Timezones", "setTimezones", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
      case 51: Scrollingnumbers("TIMZ*",(sizeof(nbPrograms)+1),currentMenuItem, 1, 1);break;
      case 52: Scrollingnumbers("TIMZ*",(sizeof(nbPrograms)+1),currentMenuItem, 1, 1);break;
      case 521: Scrollingmenu("ITEM*", 3,"SR", "MANUAL", "SS", "", "", "", "",  "", "", "", currentMenuItem);break;
      case 5211: Scrollingnumbers("HOUR*",25, currentMenuItem, 1, 1); break;
      case 52111: Scrollingnumbers("MIN***",62, currentMenuItem, 0 , 1); break;
      case 5212: Scrollingnumbers("MOD***",122, currentMenuItem, -60 , 1); break;
      case 521111: Scrollingnumbers("TEMP", 42, currentMenuItem, 0, 1);break;
*/
      //-------------------------------SelectMenu5-----------------------------------------
      //État, setProgram
      case 5 :
        switch (y) {
          case 1:
            switchmenu(51); break;
          case 2:
            if (sizeof(nbPrograms) >= 2){switchmenu(52);}
            else{switchmenu(0);}
            break;
          case 3:
            if (sizeof(nbPrograms) >= 3){switchmenu(52);}
            else{switchmenu(0);}
            break;
          case 4:
            if (sizeof(nbPrograms) >= 4){switchmenu(52);}
            else{switchmenu(0);}
            break;
          case 5:
            if (sizeof(nbPrograms) >= 5){switchmenu(52);}
            else{switchmenu(0);}
            break;
          case 6: switchmenu(0); break;
        }
      break;

      //-------------------------------SelectMenu41-----------------------------------------
    //PROGRAMME DES TIMEZONES
    case 51 :
        switch(y){
          case 1 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 2);
          break;
          case 2 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 2);
          case 3 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 2);
          case 4 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 2);
          case 5 :
          lcd.noBlink();
          clearPrintTitle();
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 2);
          break;
          case 6 :
          switchmenu(5);
          break;
        }
      break;
    //-------------------------------SelectMenu52-----------------------------------------
    //SET TIMEZONES PROGRAM
    case 52 :
      if (y < Nbitems) {
        nr = y;
        switchmenu(521);
      }
      else {
        switchmenu(5);
      }
      break;
    //-------------------------------SelectMenu521-----------------------------------------
    //SET TYPE
    case 521 :
      switch(y){
        case 1:typ = SR;switchmenu(5212);break;
        case 2:typ = CLOCK;switchmenu(5211);break;
        case 3:typ = SS;switchmenu(5212);break;
        case 4:switchmenu(5);break;
      }
      break;
    //-------------------------------SelectMenu5211-----------------------------------------
    //SET HOUR
    case 5211 :
      if (y < Nbitems) {
        hr = y;
        switchmenu(52111);
      }
      else{
        switchmenu(5);
      }
      break;
    //-------------------------------SelectMenu52111-----------------------------------------
    //SET MINUTES
    case 52111 :
      if (y < Nbitems) {
        mn = y - 1;
        switchmenu(521111);
      }
      else {
        switchmenu(5);
      }
      break;
    //-------------------------------SelectMenu5212-----------------------------------------
    //SET MOD
    case 5212 :
      if (y < Nbitems) {
        mod = y-60;
        switchmenu(52111);
      }
      else {
        switchmenu(5);
      }
    break;
    //-------------------------------SelectMenu521111-----------------------------------------
    //SET TEMP CIBLE AND UPDATE EEPROM
    case 521111 :
      if (y < Nbitems) {
        tt = y-1;
        if (typ == CLOCK){
          defineProgram(nr,typ,hr,mn,tt);
          switchmenu(5);
        }
        else{
          defineProgram(nr,typ,0,mod,tt);
          switchmenu(5);
        }
      }
      else {
        switchmenu(5);
      }
      break;
    }
  }
