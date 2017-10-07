#include "Arduino.h"


//*****************LIBRAIRIES************************
#include <EEPROM.h>
#include "GreenhouseLib.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <TimeLord.h>

//*****************DEFINITIONS***********************

//Uncomment to enable serial communication
//#define DEBUG_CLOCK
//#define DEBUG_SOLARCALC
//#define DEBUG_PROGRAM
//#define DEBUG_ROLLUP


//********************PINOUT**************************

#define BUTTON_PAD      A0  //connect this pin to an analog four button pad
#define ONE_WIRE_BUS    A1 //connect this pin to the DS18B20 data line
#define MENU_PIN        8  //link this pin to ground with an on/off switch in between
#define OPENING_PIN     6 //connect this pin to the opening relay
#define CLOSING_PIN     7 //connect this pin to the closing relay
#define TOP_SAFETY_SWITCH  3
#define BOTTOM_SAFETY_SWITCH 2


//********************OBJECTS**************************

//Create a RTC object
DS3231  rtc(SDA, SCL);                // Init the DS3231 using the hardware interface
Time  t;

//Create Timelord object
const int TIMEZONE = -5; //PST
const float LATITUDE = 45.50, LONGITUDE = -73.56; // set your position here
TimeLord myLord; // TimeLord Object, Global variable

//Create timezones objects
const int nbTimezones = 5;
Timezone timezone[nbTimezones];
Timezone &timezone1 = timezone[0];
Timezone &timezone2 = timezone[1];
Timezone &timezone3 = timezone[2];
Timezone &timezone4 = timezone[3];
Timezone &timezone5 = timezone[4];

//Create rollup object
Rollup rollup1;

//Create DS18B20 object
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Create lcd object using NewLiquidCrystal lib
#define I2C_ADDR    0x27              // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3
LiquidCrystal_I2C  lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7);

//********************VARIABLES**************************

//Time
byte program;                         //Actual program
byte lastProgram;                     //Last program
byte sunTime[6] = {};                 //actual time(sec, min, hour, day. month, year)

//Temperature
boolean failedSensor;
float targetTemp;      //target temperature for the greenhouse

//Debounce/delays/avoid repetitions in printing
unsigned long previousMillis = 0;
const long debugInterval = 1000;  //Send serial data every...
const long debouncingDelay = 80;  //button debouncing delay (range between 50 and 100 for faster or slower response)
boolean firstPrint = true;  //tells if the text on the LCD has been printed already
boolean control = true;     //tells if in mode control or menu

unsigned long ramping = 300000;
unsigned long rampingCounter;
unsigned long lastCount;
//-----------------------LCD Menu------------------
//curseur
byte currentMenuItem = 1;
//buttonstate
byte state = 0;
byte laststate = 0;
//menu
int menu = 0;
//#items/menu
byte Nbitems = 6;
char* menuitems[10] = {};
int yr; 
byte mt, dy, hr, mn, sc;
byte tz;


void setup() {

  Serial.begin(9600);//start serial communication
  initLCD(20,4);

//declare inputs
  pinMode(BUTTON_PAD, INPUT_PULLUP);
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(MENU_PIN, INPUT_PULLUP);
  pinMode(TOP_SAFETY_SWITCH, INPUT_PULLUP);
  pinMode(BOTTOM_SAFETY_SWITCH, INPUT_PULLUP);

  sensors.begin(); //start communication with temp probe
  rtc.begin(); //start communication with RTC
  
  solarCalculations();  //Calculate sunrise and sunset based on geographic localisation

  rollup1.initOutputs(VAR_TEMP,OPENING_PIN,CLOSING_PIN); //Pin 6 => opening relay; pin 7 => closing relay

// IF IT IS YOUR FIRST UPLOAD  ----->

  //Uncomment the following lines to load new parameters in EEPROM,
  //timezone1.setParametersInEEPROM(SR, -30, 20);
  //timezone2.setParametersInEEPROM(SR, 0, 21);
  //timezone3.setParametersInEEPROM(CLOCK, 12, 25, 22);
  //timezone4.setParametersInEEPROM(SS, -60, 15);
  //timezone5.setParametersInEEPROM(SS, 0, 18);
  //rollup1.setParametersInEEPROM(0, 1, 25, 25, 5, 5, true);

//Then put back the comment markers and upload again to allow new settings

  //PARAMETERS :
  //Temperature mod : +0C (targetTemp is declared in the routine function)
  //hysteresis : 1C
  //Rotation time (Up): 25 sec
  //Rotation time (Down): 25 sec
  //Increments : 5
  //Pause between rotation : 5
  //Safety mode : ON
  
  timezone1.loadEEPROMParameters();
  timezone2.loadEEPROMParameters();
  timezone3.loadEEPROMParameters();
  timezone4.loadEEPROMParameters();
  timezone5.loadEEPROMParameters();
  rollup1.loadEEPROMParameters(); //read EEPROM values to define member variables

  rightNowParameters();  //actual time, timezone and targetTemp;

  /*
  if (rollup1._safety == true) {
    Serial.println("safety mode selected");
        attachInterrupt(digitalPinToInterrupt(TOP_SAFETY_SWITCH), reachTop, FALLING);
        attachInterrupt(digitalPinToInterrupt(BOTTOM_SAFETY_SWITCH), reachBottom, FALLING);
    }*/
}
void loop() {
//MODE MENU
  if (digitalRead(MENU_PIN) == LOW) {
    //Si passe du mode contrôle au mode menu...
    if (control == true) {
      control = false;
      displayMenu(menu);
    }
    //Programme de menu
    Menu(menu);
    delay(debouncingDelay);
  }

//MODE CONTROLE
  else if (digitalRead(MENU_PIN) == HIGH) {
    if (control == false) {
      lcd.clear();
      lcd.noBlink();
      control = true;
    }
    getDateAndTime();
    selectActualProgram();
    //Définition de la température d'activation des sorties
    startRamping();
    //Affichage
    lcdDisplay();
    //Protocole de controle
    rollup1.routine(targetTemp, greenhouseTemperature()); //execute the routine programme
  }

  printState(); //Print parameters of the rollup through serial line (if DEBUG_ROLLUP is defined)
  
  timezone1.EEPROMUpdate();
  timezone2.EEPROMUpdate();
  timezone3.EEPROMUpdate();
  timezone4.EEPROMUpdate();
  timezone5.EEPROMUpdate();
  rollup1.EEPROMUpdate(); //Update EEPPROM settings if necessary
}

void reachTop(){
 static unsigned long last_interrupt_time1 = 0;
 unsigned long interrupt_time = millis();
 // If interrupts come faster than 200ms, assume it's a bounce and ignore
 if (interrupt_time - last_interrupt_time1 > 1000)
 {
   
  rollup1._incrementCounter = rollup1._increments;
  digitalWrite(OPENING_PIN, LOW);
  digitalWrite(CLOSING_PIN, LOW);
  Serial.println("REACHED TOP");
 }
 last_interrupt_time1 = interrupt_time;
}

void reachBottom(){
 static unsigned long last_interrupt_time2 = 0;
 unsigned long interrupt_time = millis();
 // If interrupts come faster than 200ms, assume it's a bounce and ignore
 if (interrupt_time - last_interrupt_time2 > 1000)
 {
  rollup1._incrementCounter = 1;
  digitalWrite(OPENING_PIN, LOW);
  digitalWrite(CLOSING_PIN, LOW);
  Serial.println("REACHED BOTTOM");
 }
 last_interrupt_time2 = interrupt_time;
}

void initLCD(byte length, byte width){
  lcd.begin(length, width);
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
}

void solarCalculations(){
  initTimeLord();
  //Première lecture d'horloge pour définir le lever et coucher du soleil
  getDateAndTime();
  //Définition du lever et du coucher du soleil
  setSunrise();
  setSunset();
}

void initTimeLord(){
  myLord.TimeZone(TIMEZONE * 60);
  myLord.Position(LATITUDE, LONGITUDE);
  myLord.DstRules(3,2,11,1,60); // DST Rules for USA
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
  
  #ifdef DEBUG_CLOCK
  for(int x = 0; x < sizeof(sunTime); x++){
    Serial.print(sunTime[x]);
    Serial.print(":");
  }
  Serial.println("");
  #endif
}


void setSunrise(){
  //Définit l'heure du lever et coucher du soleil
  myLord.SunRise(sunTime); ///On détermine l'heure du lever du soleil
  myLord.DST(sunTime);//ajuster l'heure du lever en fonction du changement d'heure
  Timezone::sunRise[HEURE] = (short)sunTime[HEURE];
  Timezone::sunRise[MINUTE] = (short)sunTime[MINUTE];

  #ifdef DEBUG_SOLARCALC
    Serial.print("lever du soleil :");Serial.print(Timezone::sunRise[HEURE]);  Serial.print(":");  Serial.println(Timezone::sunRise[MINUTE]);
  #endif
}

void setSunset(){
  // Sunset: 
  myLord.SunSet(sunTime); // Computes Sun Set. Prints:
  myLord.DST(sunTime);
  Timezone::sunSet[HEURE] = (short)sunTime[HEURE];
  Timezone::sunSet[MINUTE] = (short)sunTime[MINUTE];
  #ifdef DEBUG_SOLARCALC
    Serial.print("coucher du soleil :");  Serial.print(Timezone::sunSet[HEURE]);  Serial.print(":");  Serial.println(Timezone::sunSet[MINUTE]);
  #endif
}
void rightNowParameters(){
  //Exécution du programme
  getDateAndTime();
  selectActualProgram();
  setTempCible();
}

void selectActualProgram(){
  //Sélectionne le programme en cour
    #ifdef DEBUG_PROGRAM
      Serial.println("----");
      Serial.print ("Heure actuelle ");Serial.print(" : ");Serial.print(sunTime[HEURE] );Serial.print(" : ");Serial.println(sunTime[MINUTE]);
    #endif
    for (byte y = 0; y < (nbTimezones-1); y++){
      
    #ifdef DEBUG_PROGRAM
      Serial.print ("Programme "); Serial.print(y+1);Serial.print(" : ");Serial.print(P[y][HEURE]);Serial.print(" : ");Serial.println(P[y][MINUTE]);
    #endif
      if (((sunTime[HEURE] == timezone[y]._hour)  && (sunTime[MINUTE] >= timezone[y]._min))||((sunTime[HEURE] > timezone[y]._hour) && (sunTime[HEURE] < timezone[y+1]._hour))||((sunTime[HEURE] == timezone[y+1]._hour)  && (sunTime[MINUTE] <timezone[y+1]._min))){
          program = y+1;
        }
    }
    
    #ifdef DEBUG_PROGRAM
      Serial.print ("Programme ");Serial.print(nbTimezones);Serial.print(" : ");Serial.print(P[nbTimezones-1][HEURE]);Serial.print(" : ");Serial.println(P[nbTimezones-1][MINUTE]);
    #endif
    
    if (((sunTime[HEURE] == timezone[nbTimezones-1]._hour)  && (sunTime[MINUTE] >= timezone[nbTimezones-1]._min))||(sunTime[HEURE] > timezone[nbTimezones-1]._hour)||(sunTime[HEURE] < timezone[0]._hour)||((sunTime[HEURE] == timezone[0]._hour)  && (sunTime[MINUTE] < timezone[0]._min))){
      program = nbTimezones;
    }
    #ifdef DEBUG_PROGRAM
      Serial.print ("Program is : ");
      Serial.println(program);
    #endif
}

void setTempCible(){
  targetTemp = timezone[program-1]._targetTemp;
}

float greenhouseTemperature(){
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    Serial.println(temp);
    if((temp < -20.00)||(temp > 80)){
      temp = targetTemp+2;
      return temp;
      failedSensor = true;
    }
    else{
      return temp;
      failedSensor = false;
    }
}

void lcdDisplay() {
  if(control == false){
    lcd.clear();
  }
  lcdPrintTemp();
  lcdPrintTempCible();
  lcdPrintRollups();
  lcdPrintTime();
  //lcdPrintOutputsStatus();
}


void lcdPrintRollups(){
    lcd.setCursor(0, 1); lcd.print(F("         "));
    lcd.setCursor(0, 1); lcd.print(F("Rollup: "));
    lcd.setCursor(7, 1);
    if(rollup1._incrementCounter == 100){lcd.print(F(""));}
    else{lcd.print(rollup1._incrementCounter);}
 }
void lcdPrintTemp(){
    float temperature = greenhouseTemperature();
    lcd.setCursor(0,0); lcd.print(F("         "));
    lcd.setCursor(0,0); lcd.print(temperature); lcd.print(F("C"));
}

void lcdPrintTempCible(){
      lcd.setCursor(9,0);lcd.print("|");lcd.print(targetTemp + rollup1._tempParameter); lcd.print(F("C "));
}
void lcdPrintTime(){
    lcd.setCursor(9,1); lcd.print(F("|(P")); lcd.print(program); lcd.print(F(":"));
    lcd.setCursor(14,1); lcdPrintDigits(sunTime[HEURE]); lcd.print(F(":")); lcdPrintDigits(sunTime[MINUTE]);
    lcd.setCursor(19,1); lcd.print(F(")"));
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

void startRamping(){
  //Définition des variables locales
  float newTargetTemp;

  newTargetTemp = timezone[program-1]._targetTemp;

  if (newTargetTemp > targetTemp){
    unsigned long rampingCounter = millis();
    //Serial.println(rampingCounter);
    //Serial.println(rampingCounter - lastCount);
    if(rampingCounter - lastCount > ramping) {
      lastCount = rampingCounter;
      targetTemp += 0.5;
    }
  }
  else if (newTargetTemp < targetTemp){
    unsigned long rampingCounter = millis();
    if(rampingCounter - lastCount > ramping) {
      lastCount = rampingCounter;
      targetTemp -= 0.5;
    }
  }
}

void printState(){
  #ifdef DEBUG_ROLLUP
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= debugInterval) {
    previousMillis = currentMillis;
    Serial.print(F("Temperature : "));
    Serial.println(greenhouseTemperature());
    Serial.print(F("Parametre de temperature : "));
    Serial.println(rollup1._tempParameter);
    Serial.print(F("Hysteresis : "));
    Serial.println(rollup1._hyst);
    Serial.print(F("Rotation(haut) : "));
    Serial.println(rollup1._rotationUp);
    Serial.print(F("Rotation(bas) : "));
    Serial.println(rollup1._rotationDown);
    Serial.print(F("Pause : "));
    Serial.println(rollup1._pause);
    Serial.print(F("Increments : "));
    Serial.println(rollup1._increments);
    Serial.print(F("Securite : "));
    Serial.println(rollup1._safety);
    Serial.println("");
  }
  #endif
}
/*
void createMenu(){
  _menu *r,*s1,*s2,*s3,*s4, *s5, *s6, *s7, *s8;

  menu.begin(&lcd,20,4); //declare lcd object and screen size to menwiz lib

  r = menu.addMenu(MW_ROOT,NULL,F("Menu principal"));

  /***************************************************
  *******************MENU TIMEZONES*********************
  ***************************************************/
/*
  s1 = menu.addMenu(MW_SUBMENU,r, F("Timezones"));
      s3 = menu.addMenu(MW_VAR, s2, F("Type(SR/clock/SS)"));
        s3 ->addVar(MW_LIST,&timezone1._type);
        s3 ->addItem(MW_LIST, F("1"));
        s3 ->addItem(MW_LIST, F("2"));
        s3 ->addItem(MW_LIST, F("3"));
      s3 = menu.addMenu(MW_VAR, s2, F("Type(SR/clock/SS)"));
        s3 ->addVar(MW_LIST,&timezone1._type);
        s3 ->addItem(MW_LIST, F("SUNRISE"));
        s3 ->addItem(MW_LIST, F("CLOCK"));
        s3 ->addItem(MW_LIST, F("SUNSET"));
      s3 = menu.addMenu(MW_VAR, s2, F("Hour(clock only)"));
        s3 ->addVar(MW_AUTO_INT,&timezone1._hour,0,24,1);
      s3 = menu.addMenu(MW_VAR, s2, F("Minut(clock only)"));
        s3 ->addVar(MW_AUTO_INT,&timezone1._min,0,60,1);
      s3 = menu.addMenu(MW_VAR, s2, F("Mod(SR/SS only)"));
        s3 ->addVar(MW_AUTO_INT,&timezone1._mod,-60,60,1);
      s3 = menu.addMenu(MW_VAR, s2, F("Target temperature"));
        s3 ->addVar(MW_AUTO_FLOAT,&timezone1._targetTemp,0,40,1);
          
  /***************************************************
  *******************MENU ROLLUPS*********************
  ***************************************************/
/*
    s1 = menu.addMenu(MW_SUBMENU,r, F("Rollups"));
      s2 = menu.addMenu(MW_VAR, s1, F("State"));
        s2 ->addVar(MW_ACTION, displayRollup);
        s2 ->setBehaviour(MW_ACTION_CONFIRM, false);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. temp mod"));
        s2 ->addVar(MW_AUTO_FLOAT,&rollup1._tempParameter,-10,10,1);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. hyst"));
        s2 ->addVar(MW_AUTO_FLOAT,&rollup1._hyst,0,5,1);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. rotation up"));
        s2 ->addVar(MW_AUTO_INT,&rollup1._rotationUp,0,240,1);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. rotation down"));
        s2 ->addVar(MW_AUTO_INT,&rollup1._rotationDown,0,240,1);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. increments"));
        s2 ->addVar(MW_AUTO_INT,&rollup1._increments,0,5,1);
      s2 = menu.addMenu(MW_VAR, s1, F("Def. security"));
        s2 ->addVar(MW_BOOLEAN,&rollup1._safety);


    menu.addUsrNav(buttonState, 4);//Use 4 custom buttons
}
void displayTimezone1(){}

void displayTemp(){
  while(buttonState() != MW_BTE){
    if(firstPrint == true){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Capteurs");
      lcd.setCursor(0,1);
      lcd.print("Temp :");
      lcd.print(greenhouseTemperature());
      lcd.print("C");
      firstPrint = false;
    }
  }
  if(firstPrint == false){firstPrint = true;};
}

void displayRollup(){
  while(buttonState() != MW_BTE){
    if(firstPrint == true){
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(F("Inc:")); lcd.print(rollup1._increments); lcd.print(F(""));; lcd.setCursor(8,0);lcd.print(F("|P:")); lcd.print(rollup1._pause); lcd.print(F("s"));
      lcd.setCursor(0, 1); lcd.print(F("R:")); lcd.print(rollup1._rotationUp); lcd.print(F("s")); lcd.setCursor(8,1);lcd.print(F("|T:")); lcd.print(rollup1._tempParameter);lcd.print(F("C"));

      firstPrint = false;
    }
  }
  if(firstPrint == false){firstPrint = true;};
}

int buttonState() {
  int x = readButtonState();
}

int readButtonState(){
  int x = analogRead(A0);
  if (x < 50)       {    return MW_BTD;}    //Button confirm
  else if (x < 75) {    return MW_BTU;}    //Button up
  else if (x < 100) {    return MW_BTC;}    //Button down
  else if (x < 120) {    return MW_BTE;}    //Button exit
  else              {    return MW_BTNULL;} //No button
}
*/


//**************************************************************
//****************    MACROS - MENU     ***********************
//**************************************************************

void Menu(int x) {

  int a = analogRead(A0);
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

void programPrint(byte x){
    lcd.noBlink();
    clearPrintTitle();
    lcd.setCursor(0, 1);lcd.print(F("Type: "));
      switch(timezone[x]._type){
        case SR: lcd.print(F("SUNRISE"));break;
        case CLOCK: lcd.print(F("MANUAL"));break;
        case SS: lcd.print(F("SUNSET"));break;
      }
    lcd.setCursor(0, 2);lcd.print(F("HD: "));lcdPrintDigits(timezone[x]._hour);lcd.print(F(":"));lcdPrintDigits(timezone[x]._min);
    if(x<(nbTimezones-1)){
      lcd.setCursor(9, 2);lcd.print(F("| HF: "));lcdPrintDigits(timezone[x+1]._hour);lcd.print(F(":"));lcdPrintDigits(timezone[x+1]._min);
    }
    else{
      lcd.setCursor(9, 2);lcd.print(F("| HF: "));lcdPrintDigits(timezone[0]._hour);lcd.print(F(":"));lcdPrintDigits(timezone[0]._min);
    }
    lcd.setCursor(0, 3);lcd.print(F("Temp. cible: "));lcd.print(timezone[x]._targetTemp);lcd.print(F("C"));
}

//-----------------DISPLAY-------------------------
//Scrollingmenu(Nbitems, Item1, item2, item3, item4, item5, Item6, item7, item8, item9, item10, currentMenuItem); //leave variable "Itemx" blank if it doesnt exist
//Scrollingnumbers(Nbitems, currentMenuItem, starting number, multiplicator);

void displayMenu(int x) {
  switch (x) {
    case 0: Scrollingmenu("", 4, "Capteurs", "Date/heure", "Rollups", "Programmes", "", "", "", "", "", "", currentMenuItem); break;
    case 1: Scrollingmenu("TIME*", 6, "Date", "Time", "SetDOW", "Set date", "Set time", "back", "", "", "", "", currentMenuItem); break;

    case 11: Scrollingmenu("DOW**", 8, "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "back", "", "", currentMenuItem); break;
    case 12: Scrollingnumbers("YEAR*",11, currentMenuItem, 2016, 1);  break;
    case 121: Scrollingnumbers("MONTH", 13, currentMenuItem, 1 , 1);  break;
    case 1211: Scrollingnumbers("DAY**", 32, currentMenuItem, 1, 1); break;
    case 13: Scrollingnumbers("HOUR*",25, currentMenuItem, 1, 1); break;
    case 131: Scrollingnumbers("MIN***",62, currentMenuItem, 0 , 1); break;
    case 1311: Scrollingnumbers("SEC**",62, currentMenuItem, 0, 1); break;

    case 2: Scrollingmenu("ROLL*", 9, "Parameters", "set Increments", "set Rotation up", "set Rotation down", "set Pause", "set Mod", "set Hysteresis", "set Safety",  "back", "", currentMenuItem); break;
    case 21: Scrollingnumbers("INCR*",11, currentMenuItem, 1, 1); break;
    case 22: Scrollingnumbers("ROTA-U*", 126, currentMenuItem, 2, 2); break;
    case 23: Scrollingnumbers("ROTA-D*", 126, currentMenuItem, 2, 2); break;
    case 24: Scrollingmenu("PAUSE", 10, "5", "15", "30", "45", "60", "75", "90", "120", "240", "back", currentMenuItem); break;
    case 25: Scrollingnumbers("MOD**", 22, currentMenuItem, -10, 1); break;
    case 26: Scrollingnumbers("HYST*", 6, currentMenuItem, 1, 1); break;
    case 27: Scrollingmenu("SAFTY", 3, "Oui", "Non", "back", "", "", "", "",  "", "", "", currentMenuItem); break;

    case 5: Scrollingmenu("PROG*", 3, "Timezones", "set Timezones", "back", "", "", "", "",  "", "", "", currentMenuItem); break;
    case 51: Scrollingnumbers("TIMZ*",(nbTimezones+1),currentMenuItem, 1, 1);break;
    case 52: Scrollingnumbers("TIMZ*",(nbTimezones+1),currentMenuItem, 1, 1);break;
    case 521: Scrollingmenu("TIMZ*", 4, "set Type", "set Time", "set Temp", "back", "", "", "",  "", "", "", currentMenuItem); break;
    case 5211: Scrollingmenu("TYPE*", 4,"SR", "MANUAL", "SS", "back", "", "", "",  "", "", "", currentMenuItem);break;
    case 5212: Scrollingnumbers("HOUR*",25, currentMenuItem, 1, 1); break;
    case 52121: Scrollingnumbers("MIN***",62, currentMenuItem, 0 , 1); break;
    case 5213: Scrollingnumbers("MOD***",122, currentMenuItem, -60 , 1); break;
    case 5214: Scrollingnumbers("TEMP", 42, currentMenuItem, 0, 1);break;

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
        //case 4: switchmenu(3); break;
        case 4: switchmenu(5); break;

 //       case 5: switchmenu(4); break;
 //       case 6: switchmenu(5); break;
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
    //
    case 2 :
      switch (y) {
        case 1:
        lcd.noBlink();
        clearPrintTitle();
        lcd.setCursor(0, 1); lcd.print(F("Inc: ")); lcd.print(rollup1._increments);lcd.setCursor(8,1);lcd.print(F("| TP: ")); lcd.print(rollup1._pause);
        lcd.setCursor(0, 2); lcd.print(F("RU: ")); lcd.print(rollup1._rotationUp); lcd.print(F("s")); lcd.setCursor(8,2);lcd.print(F("| RD: ")); lcd.print(rollup1._rotationDown); lcd.print(F("s"));
        lcd.setCursor(0, 3); lcd.print(F("H: ")); lcd.print(rollup1._hyst); lcd.print(F("C")); lcd.setCursor(8,3);lcd.print(F("| Tg: ")); lcd.print(targetTemp+rollup1._tempParameter);lcd.print("C");
        break;
        case 2:
          switchmenu(21); break;
        case 3:
          switchmenu(22); break;
        case 4:
          switchmenu(23); break;
        case 5:
          switchmenu(24); break;
        case 6:
          switchmenu(25); break;
        case 7:
          switchmenu(26); break;
        case 8:
          switchmenu(27); break;
        case 9: switchmenu(0); break;
      }
      break;
    //-------------------------------SelectMenu21--------------------
    //SET INCREMENTS
    case 21 :
      if (y < Nbitems) {
        rollup1.setIncrements(y);
        switchmenu(2);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu22-----------------------------------------
    //SET ROTATION UP
    case 22 :
      if (y < Nbitems) {
        rollup1.setRotationUp(y*2);
        switchmenu(2);
      }
      else {
        switchmenu(2);
      }
      break;
    //-------------------------------SelectMenu23-----------------------------------------
    //SET ROTATION TIME
    case 23 :
      if (y < Nbitems) {
        rollup1.setRotationDown(y*2);
        switchmenu(2);
      }
      else {
        switchmenu(2);
      }
      break;
      //-------------------------------SelectMenu24-----------------------------------------
      //SET PAUSE TIME
      case 24:
        switch (y) {
          case 1: rollup1.setPause(5); switchmenu(2);break;
          case 2: rollup1.setPause(15); switchmenu(2); break;
          case 3: rollup1.setPause(30); switchmenu(2); break;
          case 4: rollup1.setPause(45); switchmenu(2); break;
          case 5: rollup1.setPause(60); switchmenu(2); break;
          case 6: rollup1.setPause(75); switchmenu(2); break;
          case 7: rollup1.setPause(90); switchmenu(2); break;
          case 8: rollup1.setPause(120); switchmenu(2); break;
          case 9: rollup1.setPause(240); switchmenu(2); break;
          case 10: switchmenu(2); break;
          }
        break;
      //-------------------------------SelectMenu25-----------------------------------------
      //SET MODIFICATOR
      case 25 :
        if (y < Nbitems) {
          rollup1.setTemp(y-1);
          switchmenu(2);
        }
        else {
          switchmenu(2);
        }
      break;
      //-------------------------------SelectMenu26-----------------------------------------
      //SET HYSTERESIS
      case 26 :
        if (y < Nbitems) {
          rollup1.setHyst(y);
          switchmenu(2);
        }
        else {
          switchmenu(2);
        }
      break;
      //-------------------------------SelectMenu27-----------------------------------------
      //SET SAFETY AND UPDATE EEPROM
      case 27 :
        switch (y){
          case 1: rollup1.setSafety(true); switchmenu(2);break;
          case 2: rollup1.setSafety(false); switchmenu(2);break;
          case 3: switchmenu(2);break;
        }
      break;
      //-------------------------------SelectMenu5-----------------------------------------
      
      //Timezones, setTimezones
      case 5 :
        switch (y) {
          case 1:
            switchmenu(51); break;
          case 2:
            switchmenu(52); break;
          case 3: switchmenu(0); break;
        }
      break;

      //-------------------------------SelectMenu41-----------------------------------------

      //PROGRAMME DES TIMEZONES
      case 51 :
          switch(y){
            case 1 :
                programPrint(0);
            break;
            case 2 :
                programPrint(1);
            break;
            case 3 :
                programPrint(2);
            break;
            case 4 :
                programPrint(3);
            break;
            case 5 :
                programPrint(4);
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
        tz = y-1;
        switchmenu(521);
      }
      else {
        switchmenu(5);
      }
      break;
    //-------------------------------SelectMenu521-----------------------------------------
    //SET PARAMETERS
    case 521 :
      switch(y){
        case 1:switchmenu(5211);break;
        case 2:if(timezone[tz]._type == CLOCK){switchmenu(5212);}else{switchmenu(5213);}break;
        case 3:switchmenu(5214);break;
        case 4:switchmenu(52);break;
      }
      break;
    //-------------------------------SelectMenu521-----------------------------------------
    //SET TYPE
    case 5211 :
      switch(y){
        case 1:timezone[tz].setType(SR);switchmenu(521);break;
        case 2:timezone[tz].setType(CLOCK);switchmenu(521);break;
        case 3:timezone[tz].setType(SS);break;
        case 4:switchmenu(521);break;
      }
      break;
    //-------------------------------SelectMenu5221-----------------------------------------
    //SET HOUR
    case 5212 :
      if (y < Nbitems) {
        hr = y;
        switchmenu(52121);
      }
      else{
        switchmenu(521);
      }
      break;
    //-------------------------------SelectMenu523-----------------------------------------
    //SET MINUTES
    case 52121 :
      if (y < Nbitems) {
        mn = y - 1;
        timezone[tz].setTime(hr,mn);
        switchmenu(521);
      }
      else {
        switchmenu(521);
      }
      break;
    //-------------------------------SelectMenu5222-----------------------------------------
    //SET MOD
    case 5213 :
      if (y < Nbitems) {
        timezone[tz].setMod((int)y-11);
        switchmenu(521);
      }
      else {
        switchmenu(521);
      }
    break;
    //-------------------------------SelectMenu524-----------------------------------------
    //SET TEMP CIBLE
    case 5214 :
      if (y < Nbitems) {
        timezone[tz].setTemp(y-1);
        switchmenu(521);
      }
      else {
        switchmenu(521);
      }
      break;
    }
}
