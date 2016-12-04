#include <HID.h>

#include <dht.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define dht_apin A0

dht DHT;

#define I2C_ADDR    0x22  // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3

int n = 1;

LiquidCrystal_I2C  lcd(I2C_ADDR,2,1,0,4,5,6,7);
/* 
 *  En_pin  2
 *  Rw_pin  1
 *  Rs_pin  0
 *  D4_pin  4
 *  D5_pin  5
 *  D6_pin  6
 *  D7_pin  7 
 */
//**********BLOC PROGRAMMABLE*************
//Températures critiques et hysteresis

//Rollup:
const int TEMP_ROLLUP = 20; //température d'activation des rollup
const int HYST_ROLLUP = 2;

//Ventilation:
const int TEMP_VENT = 22;
const int HYST_VENT = 2;

//Chauffage:
const int TEMP_CHAUFFAGE1 = 18;//température d'activation de la 1re fournaise
const int HYST_CHAUFFAGE1 = 2;//
const int TEMP_CHAUFFAGE2 = 15;//température d'activation de la 2e fournaise
const int HYST_CHAUFFAGE2 = 2;

//incréments d'ouverture(nombre d'incréments d'ouverture = PCT_OPEN/NB_OF_STEPS_IN_ANIMATION = 25/5 = 5 incréments d'ouverture)
const int PCT_OPEN = 25;
const int NB_OF_STEPS_IN_ANIMATION = 5; //doit être un divisible de PCT_OPEN

//temps de rotation et pauses
const int ROTATION_TIME = 4000; //temps de rotation des moteurs(en mili-secondes)
const int PAUSE_TIME = 2000; //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
const int SLEEPTIME = 1000; //temps de pause entre chaque exécution du programme(en mili-secondes)

//*************FIN DU BLOC PROGRAMMABLE**********

//Entrées
const int THERMOMETER_IN = A0;// sonde de température DHT11
//Sorties
const int ROLLUP1_POWER = 12;//relais on/off - moteur1
const int ROLLUP1_DIRECTION = 11; //relais gauche/droite - moteur1
const int ROLLUP2_POWER = 10; //relais on/off - moteur2
const int ROLLUP2_DIRECTION = 9; //relais gauche/droite - moteur2
const int FAN = 8; //relais ventilation forcée
const int CHAUFFAGE1 = 7; //relais fournaise1
const int CHAUFFAGE2 = 6; // relais fournaise2

//vocabulaire
const int CLOSE = HIGH;
const int OPEN = LOW;
const int ON = LOW;
const int OFF = HIGH;

//consignes initiales
int opened = 0;
boolean heating1 = false; //fournaise 1 éteinte par défaut
boolean heating2 = false; //fournaise 2 éteinte par défaut
boolean fan = false;//ventilation forcée éteinte par défaut
float greenhouseTemperature = 20.0; //température par défaut : 20C (ajusté après un cycle)


//********INITIALISATION DU PROGRAMME**********
void setup() {
  Serial.begin(9600);   // démarre la communication sérielle 
  Serial.println("Starting");
   lcd.begin (20,4); //démarre l'affichage LCD
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH); //allume le rétro-éclairage
  lcd.home ();      
  lcd.setCursor(0, 0);
  lcd.print("Resetting");
  
  lcd.setCursor(0, 2);             
  lcd.print("FAN:");
  lcd.setCursor(16,2);
  lcd.print("OFF");

  lcd.setCursor(0, 3);
  lcd.print("HEATING: ");
  lcd.setCursor(16,3);
  lcd.print("OFF");

  //Définition et initalisation des sorties
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
  Serial.println("Resetting position");
  opened = 100;
  for (int i=0; i<5; i++){
    closeSides();
    delay(PAUSE_TIME);
  }
  Serial.println("Resetting done");
}

//Affichage du % d'ouverture des rollup
void setOpenedStatus(int pctIncrease){
  opened += pctIncrease;
  if(opened < 0) {
    opened = 0;
  }else if (opened > 100){
    opened = 100;
  }
  Serial.print("    ");
  Serial.println(opened);
  lcd.setCursor(16, 1);
  lcd.print(opened);    
  lcd.print("% ");
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
    lcd.setCursor(0, 1);
    lcd.print("OUVERTURE... ");
    digitalWrite(ROLLUP1_POWER,ON);
    digitalWrite(ROLLUP1_DIRECTION, OPEN);
    digitalWrite(ROLLUP2_POWER,ON);
    digitalWrite(ROLLUP2_DIRECTION, OPEN);
    animate(1);
    digitalWrite(ROLLUP1_POWER,OFF);
    digitalWrite(ROLLUP2_POWER,OFF);
    Serial.println("  Done opening");
    lcd.setCursor(0, 1);
    lcd.print("ROLLUPS:     ");
    delay(PAUSE_TIME);
  }
}

//Programme de fermeture des rollups
void closeSides(){
  if (opened > 0){
    Serial.println("  Closing");
    lcd.setCursor(0, 1);
    lcd.print("FERMETURE... ");
    digitalWrite(ROLLUP1_POWER,ON);
    digitalWrite(ROLLUP1_DIRECTION, CLOSE);
    digitalWrite(ROLLUP2_POWER,ON);
    digitalWrite(ROLLUP2_DIRECTION, CLOSE);
    animate(-1);
    digitalWrite(ROLLUP1_POWER, OFF);
    digitalWrite(ROLLUP2_POWER, OFF);
    Serial.println("  Done closing");
    lcd.setCursor(0, 1);
    lcd.print("ROLLUPS:     ");
    delay(PAUSE_TIME);    
  }
}

//État de la première fournaise
void setHeater1(int heaterCommand1){
  if ((heaterCommand1 == ON) && (heating1 == false)){
    Serial.println("  Start heating1");
    heating1 = true;
  }else if ((heaterCommand1 == OFF) && (heating1 == true)){
    Serial.println("  Stop heating1");
    heating1 = false;    
    }
  }

//État de la deuxième fournaise
void setHeater2(int heaterCommand2){
  if ((heaterCommand2 == ON) && (heating2 == false)){
    Serial.println("  Start heating2");
    heating2 = true;
  }else if ((heaterCommand2 == OFF) && (heating2 == true)){
    Serial.println("  Stop heating2");
    heating2 = false;    
    }
  }

//État de la ventilation
void setFan(int fanCommand){
  if ((fanCommand == ON) && (fan == false)){
    Serial.println("  Start fan");

    fan = true;
  }else if ((fanCommand == OFF) && (fan == true)){
    Serial.println("  Stop fan");
    fan = false;
  }
}


void loop() {    

  DHT.read11(dht_apin);
  float greenhouseTemperature = DHT.temperature;
  float greenhouseHumidity = DHT.humidity;
  Serial.print(greenhouseTemperature);//Affichage de la temperature
  Serial.println ("C");
  Serial.print(greenhouseHumidity);//Affichage de l'humidité
  Serial.println ("%");  
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(greenhouseTemperature);
  lcd.print("C  ");
  lcd.setCursor(10,0);
  lcd.print("HR: ");
  lcd.print(greenhouseHumidity);
  lcd.print("%"); 
  if(fan == false){                   //Affichage de l'etat de la fan
      Serial.println("FAN: OFF");
      lcd.setCursor(0, 2);             
      lcd.print("FAN:");
      lcd.setCursor(16,2);
      lcd.print("OFF");}
  else if(fan == true){
      Serial.println("FAN: ON");
      lcd.setCursor(0,2);
      lcd.print("FAN:");
      lcd.setCursor(16,2);
      lcd.print("ON ");}
  if(heating1 == false){               //Affichage de l'etat du chauffage
      Serial.println("HEATING1: OFF");
      lcd.setCursor(0, 3);             
      lcd.print("HEATING:");
      lcd.setCursor(16,3);
      lcd.print("OFF");}
  else if(heating1 == true){
      Serial.println("HEATING1: ON");
      lcd.setCursor(0,3);
      lcd.print("HEATING: ");
      lcd.setCursor(16,3);
      lcd.print("ON ");}
  if(heating2 == false){
      Serial.println("HEATING2 : OFF");}
  else if(heating2 == true);{
      Serial.println("HEATING2 : ON");}

//Programme d'ouverture/fermeture des rollups
  if(greenhouseTemperature < TEMP_ROLLUP-(HYST_ROLLUP/2)){
    closeSides();
  }else if(greenhouseTemperature > TEMP_ROLLUP+(HYST_ROLLUP/2)){
    openSides();
  }
//Programme fournaise1
  if (greenhouseTemperature < (TEMP_CHAUFFAGE1-(HYST_CHAUFFAGE1/2))){
    setHeater1(ON);
    digitalWrite(CHAUFFAGE1, ON);
  }else if(greenhouseTemperature > (TEMP_CHAUFFAGE1+(HYST_CHAUFFAGE1/2))){
    setHeater1(OFF);
    digitalWrite(CHAUFFAGE1, OFF);
  }
//Programme fournaise2
  if (greenhouseTemperature < (TEMP_CHAUFFAGE2-(HYST_CHAUFFAGE2/2))){
    setHeater2(ON);
    digitalWrite(CHAUFFAGE2, ON);
  }else if(greenhouseTemperature > (TEMP_CHAUFFAGE2+(HYST_CHAUFFAGE2/2))){
    setHeater2(OFF);
    digitalWrite(CHAUFFAGE2, OFF);
  }
//Programme ventilation forcée
   if (greenhouseTemperature > (TEMP_VENT - (HYST_VENT/2))){
    setFan(ON);
    digitalWrite(FAN, ON);
  }else if(greenhouseTemperature < (TEMP_VENT + (HYST_VENT/2))){
    setFan(OFF);
    digitalWrite(FAN, OFF);
  }
  delay(SLEEPTIME);
}

