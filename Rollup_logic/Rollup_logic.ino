#include <OneWire.h>
#include <DallasTemperature.h>

#define Open 1
#define Close 0

#define ONE_WIRE_BUS 2
#define ROLLUP1_OPEN  12//relais on/off - moteur1
#define ROLLUP1_CLOSE  11 //relais gauche/droite - moteur1
#define FAN  8 //relais ventilation forcée
#define CHAUFFAGE1 7 //relais fournaise1
#define CHAUFFAGE2 6 // relais fournaise2

OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

const int ON = HIGH;
const int OFF = LOW;

byte opened = 100;
byte openTarget = 0;

boolean heating1 = false;                     //fournaise 1 éteinte par défaut
boolean heating2 = false;                     //fournaise 2 éteinte par défaut
boolean fan = false;                          //VENTilation forcée éteinte par défaut
float greenhouseTemperature = 20.0;               //température par défaut : 20C (ajusté après un cycle)

int TEMP_CIBLE = 20;          //température cible par défaut : 20C (ajustée après un cycle))
//temp cible pour équipement chauffage/refroidissement
int TEMP_ROLLUP1 = TEMP_CIBLE+1;
int TEMP_ROLLUP2 = TEMP_CIBLE+2;
int TEMP_ROLLUP3 = TEMP_CIBLE+3;
int TEMP_ROLLUP4 = TEMP_CIBLE+4;
int TEMP_VENTILATION = TEMP_CIBLE+5;
int TEMP_FOURNAISE1 = TEMP_CIBLE-2;
int TEMP_FOURNAISE2 = TEMP_CIBLE-4;

byte HYST_ROLLUP = 2;           //hysteresis rollup
byte HYST_VENT = 2;             //hysteresis ventilation
byte HYST_FOURNAISE1 = 2;       //hysteresis fournaise 1
byte HYST_FOURNAISE2 = 2;       //hysteresis fournaise 2

long OPENING_TIME;       //temps de rotation des moteurs pour ouvrir d'un quart de tour(en mili-secondes)
long CLOSING_TIME;       //temps de rotation des moteurs pour fermer d'un quart de tour(en mili-secondes)
long PAUSE_TIME;         //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
//incréments d'ouverture(nombre d'incréments d'ouverture = PCT_OPEN/NB_OF_STEPS_IN_ANIMATION = 25/5 = 5 incréments d'ouverture)
const int PCT_OPEN = 25;
const int NB_OF_STEPS_IN_ANIMATION = 5; //doit être un divisible de PCT_OPEN
//Autres variables
const int SLEEPTIME = 900; //temps de pause entre chaque exécution du programme(en mili-secondes)


volatile byte rollupState = LOW;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  sensors.begin();

  pinMode(ROLLUP1_OPEN, OUTPUT);
  digitalWrite(ROLLUP1_OPEN, LOW);
  pinMode(ROLLUP1_CLOSE, OUTPUT);
  digitalWrite(ROLLUP1_CLOSE, LOW);
  pinMode(CHAUFFAGE1, OUTPUT);
  digitalWrite(CHAUFFAGE1, LOW);
  pinMode(CHAUFFAGE2, OUTPUT);
  digitalWrite(CHAUFFAGE2, LOW);
  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);
  attachInterrupt(digitalPinToInterrupt(2), confirmOpening, CHANGE);

  //Remise à niveau des rollup
  Reset();  
}

void loop() {
  // put your main code here, to run repeatedly:
  //--------------DS18B20--------------------
  sensors.requestTemperatures();
  greenhouseTemperature = sensors.getTempCByIndex(0);
  //--------------Relais--------------------
  
  //Programme d'ouverture/fermeture des rollups à 4 paliers
  if ((opened = 0)&&(greenhouseTemperature > TEMP_ROLLUP1)){
    openTarget = 25;
    openSides();
  }
  else if ((opened = 1)&&(greenhouseTemperature < (TEMP_ROLLUP1 - HYST_ROLLUP))) {
    openTarget = 0;
    closeSides();
  }
  else if ((opened = 1)&&(greenhouseTemperature > TEMP_ROLLUP2)){
    openTarget = 50;
    openSides();
  }
  else if ((opened = 2)&&(greenhouseTemperature < (TEMP_ROLLUP2 - HYST_ROLLUP))) {
    openTarget = 25;
    closeSides();
  }
  else if ((opened = 2)&&(greenhouseTemperature > TEMP_ROLLUP3)){
    openTarget = 75;
    openSides();
  }
  else if ((opened = 3)&&(greenhouseTemperature < (TEMP_ROLLUP3 - HYST_ROLLUP))) {
    openTarget = 50;
    closeSides();
  }
  else if ((opened = 3)&&(greenhouseTemperature > TEMP_ROLLUP4)){
    openTarget = 100;
    openSides();
  }
  else if ((opened = 4)&&(greenhouseTemperature < (TEMP_ROLLUP4 - HYST_ROLLUP))) {
    openTarget = 75;
    closeSides();
  }


  //Programme fournaise1
  if (greenhouseTemperature < TEMP_FOURNAISE1) {
    setHeater1(ON);
  }
  else if (greenhouseTemperature > (TEMP_FOURNAISE1 + HYST_FOURNAISE1)) {
    setHeater1(OFF);
  }
  //Programme fournaise2
  if (greenhouseTemperature < TEMP_FOURNAISE2) {
    setHeater2(ON);
  }
  else if (greenhouseTemperature > (TEMP_FOURNAISE2 + HYST_FOURNAISE2)) {
    setHeater2(OFF);
  }
  //Programme ventilation forcée
  if ((greenhouseTemperature > TEMP_VENTILATION)&&(rollupState == HIGH)) {
    setFan(ON);
  } else if ((greenhouseTemperature < (TEMP_VENTILATION - HYST_VENT))||(rollupState == LOW)) {
    setFan(OFF);
  }
}



void Reset(){
    Serial.println(F("Resetting position"));
  for (int i = 0; i < 5; i++) {
    closeSides();
  }
  Serial.println(F("Resetting done"));
}

//Affichage du % d'ouverture des rollup
void setOpenedStatus(int pctIncrease) {
  opened += pctIncrease;
  if (opened < 0) {
    opened = 0;
  } else if (opened > 100) {
    opened = 100;
  }
  Serial.print(F("    "));
  Serial.println(opened);
}

//Exécution de la séquence d'ouverture/fermeture
void animate(int movement, int action) {
  for (int i = 0; i < NB_OF_STEPS_IN_ANIMATION; i++) {
    if (action == Open){
      delay(OPENING_TIME / NB_OF_STEPS_IN_ANIMATION);}
    else if (action == Close){
      delay(CLOSING_TIME / NB_OF_STEPS_IN_ANIMATION);}  
    setOpenedStatus(movement * PCT_OPEN / NB_OF_STEPS_IN_ANIMATION);
  }
}

//Programme d'ouverture des rollup
void openSides() {
  if (opened < 100) {
    Serial.println(F(""));
    Serial.println(F("  Opening"));
    digitalWrite(ROLLUP1_OPEN, ON);
    //digitalWrite(ROLLUP2_OPEN, ON);
    animate(1, Open);
    digitalWrite(ROLLUP1_OPEN, OFF);
    //digitalWrite(ROLLUP2_OPEN, OFF);
    Serial.println(F("  Done opening"));
    delay(PAUSE_TIME);
  }
}

//Programme de fermeture des rollups
void closeSides() {
  if (opened > 0) {
    Serial.println(F(""));
    Serial.println(F("  Closing"));
    //digitalWrite(ROLLUP2_CLOSE, ON);
    animate(-1, Close);
    digitalWrite(ROLLUP1_CLOSE, OFF);
    //digitalWrite(ROLLUP2_CLOSE, OFF);
    Serial.println(F("  Done closing"));
    delay(PAUSE_TIME);
  }
}


//État de la première fournaise
void setHeater1(int heaterCommand1) {
  if ((heaterCommand1 == ON) && (heating1 == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start heating1"));
    heating1 = true;
    digitalWrite(CHAUFFAGE1, ON);
  } else if ((heaterCommand1 == OFF) && (heating1 == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop heating1"));
    heating1 = false;
    digitalWrite(CHAUFFAGE1, OFF);
  }
}

//État de la deuxième fournaise
void setHeater2(int heaterCommand2) {
  if ((heaterCommand2 == ON) && (heating2 == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start heating2"));
    heating2 = true;
    digitalWrite(CHAUFFAGE2, ON);
  } else if ((heaterCommand2 == OFF) && (heating2 == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop heating2"));
    heating2 = false;
    digitalWrite(CHAUFFAGE2, OFF);
  }
}

//État de la ventilation
void setFan(int fanCommand) {
  if ((fanCommand == ON) && (fan == false)) {
    Serial.println(F(""));
    Serial.println(F("  Start fan"));
    fan = true;
    digitalWrite(FAN, ON);
  } else if ((fanCommand == OFF) && (fan == true)) {
    Serial.println(F(""));
    Serial.println(F("  Stop fan"));
    fan = false;
    digitalWrite(FAN, OFF);
  }
}

void confirmOpening() {
  rollupState =! rollupState;
}


