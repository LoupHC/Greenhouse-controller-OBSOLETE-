//--------------LCD 20x4--------------------
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR    0x22  // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN     3
LiquidCrystal_I2C  lcd(I2C_ADDR,2,1,0,4,5,6,7);
/* En_pin  2, Rw_pin  1, Rs_pin  0, D4_pin  4, D5_pin  5, D6_pin  6, D7_pin  7 
 */
//-------------------------Horloge-----------------
#include <DS3231.h>
DS3231  rtc(SDA, SCL);                // Init the DS3231 using the hardware interface
Time  t;                        // Init a Time-data structure
//--------------DHT11--------------------
#include <DHT.h>
#define DHTPIN A1
#define DHTTYPE           DHT11         // Uncomment the type of sensor in use:
DHT dht(DHTPIN, DHTTYPE);
//--------------Relais--------------------
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
float TEMP_CIBLE = 20.0;              //température cible par défaut : 20C (ajusté après un cycle))
float TEMP_ROLLUP = 20;
float TEMP_VENT = 22.0;
float TEMP_CHAUFFAGE1 = 18.0;
float TEMP_CHAUFFAGE2 = 15.0;
int PROGRAMME = 1;

//**********BLOC PROGRAMMABLE*************
//Programmes de températures
const int HP1 = 7;                  //Heure d'exécution du premier programme
const int MP1 = 30;                   //Minutes d'exécution du premier programme
const float TEMP_CIBLEP1 = 20.0;            //Température cible du premier programme
const float TEMP_ROLLUPP1 = (TEMP_CIBLEP1);
const float TEMP_VENTP1 = (TEMP_CIBLEP1 + 2.0);
const float TEMP_CHAUFFAGE1P1 = (TEMP_CIBLEP1 - 2.0);
const float TEMP_CHAUFFAGE2P1 = (TEMP_CIBLEP1 - 5.0);

const int HP2 = 11;                   //Heure d'exécution du deuxième programme
const int MP2 = 0;                  //Minutes d'exécution du deuxième programme
const float TEMP_CIBLEP2 = 24.0;            //Température cible du deuxième programme
const float TEMP_ROLLUPP2 = (TEMP_CIBLEP2);
const float TEMP_VENTP2 = (TEMP_CIBLEP2 + 2.0);
const float TEMP_CHAUFFAGE1P2 = (TEMP_CIBLEP2 - 2.0);
const float TEMP_CHAUFFAGE2P2 = (TEMP_CIBLEP2 - 5.0);

const int HP3 = 16;                   //Heure d'exécution du troisième programme
const int MP3 = 9;                  //Minutes d'exécution du troisième programme
const float TEMP_CIBLEP3 = 18.0;           //Température cible du troisième programme
const float TEMP_ROLLUPP3 = (TEMP_CIBLEP3);
const float TEMP_VENTP3 = (TEMP_CIBLEP3 + 2.0);
const float TEMP_CHAUFFAGE1P3 = (TEMP_CIBLEP3 - 2.0);
const float TEMP_CHAUFFAGE2P3 = (TEMP_CIBLEP3 - 5.0);

//Rollup:
//Températures critiques et hysteresis
//Rollup:
const int HYST_ROLLUP = 2;

//Ventilation:
const int HYST_VENT = 2;

//Chauffage:
const int HYST_CHAUFFAGE1 = 2;//
const int HYST_CHAUFFAGE2 = 2;

//incréments d'ouverture(nombre d'incréments d'ouverture = PCT_OPEN/NB_OF_STEPS_IN_ANIMATION = 25/5 = 5 incréments d'ouverture)
const int PCT_OPEN = 25;
const int NB_OF_STEPS_IN_ANIMATION = 5; //doit être un divisible de PCT_OPEN

//temps de rotation et pauses
const int ROTATION_TIME = 4000; //temps de rotation des moteurs(en mili-secondes)
const int PAUSE_TIME = 2000; //temps d'arrêt entre chaque ouverture/fermeture(en mili-secondes)
const int SLEEPTIME = 1000; //temps de pause entre chaque exécution du programme(en mili-secondes)

//*************FIN DU BLOC PROGRAMMABLE**********


//********INITIALISATION DU PROGRAMME**********
void setup() {
  Serial.begin(9600);   // démarre la communication sérielle
  Serial.println("");
  Serial.println("Starting");
  Serial.println("-----------------------");
//--------------LCD 20x4--------------------
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
//--------------Horloge--------------------
  rtc.begin();                      //Démarre la communication avec l'horloge
//--------------DHT11--------------------
  dht.begin(); 
//--------------General setup-------------------
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

void loop() {    
//-----------------Horloge-----------------
  t = rtc.getTime();
  int heure = t.hour;
  int minutes = t.min;
 if (((heure >= HP1)  && (heure < HP2))  && (minutes >= MP1)){
    TEMP_CIBLE = TEMP_CIBLEP1;
    TEMP_ROLLUP = TEMP_ROLLUPP1; 
    TEMP_VENT = TEMP_VENTP1; 
    TEMP_CHAUFFAGE1 = TEMP_CHAUFFAGE1P1;
    TEMP_CHAUFFAGE2 = TEMP_CHAUFFAGE2P1;;
    PROGRAMME = 1;
  }
  else if(((heure >= HP2)  && (heure < HP3))  && (minutes >= MP2)){
    TEMP_CIBLE = TEMP_CIBLEP2;
    TEMP_ROLLUP = TEMP_ROLLUPP2; 
    TEMP_VENT = TEMP_VENTP2; 
    TEMP_CHAUFFAGE1 = TEMP_CHAUFFAGE1P2;
    TEMP_CHAUFFAGE2 = TEMP_CHAUFFAGE2P2;
    PROGRAMME = 2;
  }
  else if((heure >= HP3)  && (minutes >= MP3)){
    TEMP_CIBLE = TEMP_CIBLEP3;
    TEMP_ROLLUP = TEMP_ROLLUPP3; 
    TEMP_VENT = TEMP_VENTP3; 
    TEMP_CHAUFFAGE1 = TEMP_CHAUFFAGE1P3;
    TEMP_CHAUFFAGE2 = TEMP_CHAUFFAGE2P3;;
    PROGRAMME = 3;
  } 
  //--------------DHT11--------------------
  float greenhouseTemperature = dht.readTemperature();
  float greenhouseHumidity = dht.readHumidity();
//--------------Relais--------------------
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
//--------------Affichage sériel--------------------
    Serial.println("");
    Serial.println("-----------------------");
    Serial.print(rtc.getDOWStr());
    Serial.print(",  ");
    // Send date
    Serial.print(rtc.getDateStr());
    Serial.print(" - ");
    // Send time
    Serial.println(rtc.getTimeStr());
    Serial.print("PROGRAMME : ");
    Serial.println(PROGRAMME);
  
    Serial.println("-----------------------");    
    Serial.print("Temperature cible :");
    Serial.print(TEMP_CIBLE);
    Serial.println(" C");

    Serial.print("Temperature actuelle: ");
    Serial.print(greenhouseTemperature);
    Serial.println(" C");
    Serial.print("Humidite: ");
    Serial.print(greenhouseHumidity);
    Serial.println("%");
    Serial.println("-----------------------"); 
    if(fan == false){
    Serial.println("FAN: OFF");}
    else if(fan == true){
    Serial.println("FAN: ON");}
    if(heating1 == false){
    Serial.println("HEATING1: OFF");}
    else if(heating1 == true){
    Serial.println("HEATING1: ON");}
    if(heating2 == false){
    Serial.println("HEATING2 : OFF");}
    else if(heating2 == true){
    Serial.println("HEATING2 : ON");}
//--------------Affichage LCD--------------------
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(greenhouseTemperature);
  lcd.print("C  ");
  lcd.setCursor(10,0);
  lcd.print("HR: ");
  lcd.print(greenhouseHumidity);
  lcd.print("%"); 
  if(fan == false){
      lcd.setCursor(0, 2);             
      lcd.print("FAN:");
      lcd.setCursor(16,2);
      lcd.print("OFF");}
  else if(fan == true){
      lcd.setCursor(0,2);
      lcd.print("FAN:");
      lcd.setCursor(16,2);
      lcd.print("ON ");}
  if(heating1 == false){
      lcd.setCursor(0, 3);             
      lcd.print("HEATING:");
      lcd.setCursor(16,3);
      lcd.print("OFF");}
  else if(heating1 == true){
      lcd.setCursor(0,3);
      lcd.print("HEATING: ");
      lcd.setCursor(16,3);
      lcd.print("ON ");}
//--------------fin du cycle--------------------
  delay(SLEEPTIME);
}

//--------------SOUS-PROGRAMMES--------------------
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
    Serial.println("");  
    Serial.println("  Opening");
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
    Serial.println("");  
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
    Serial.println("");  
    Serial.println("  Start heating1");
    heating1 = true;
  }else if ((heaterCommand1 == OFF) && (heating1 == true)){
    Serial.println("");  
    Serial.println("  Stop heating1");
    heating1 = false;    
    }
  }

//État de la deuxième fournaise
void setHeater2(int heaterCommand2){
  if ((heaterCommand2 == ON) && (heating2 == false)){
    Serial.println("");  
    Serial.println("  Start heating2");
    heating2 = true;
  }else if ((heaterCommand2 == OFF) && (heating2 == true)){
    Serial.println("");  
    Serial.println("  Stop heating2");
    heating2 = false;    
    }
  }

//État de la ventilation
void setFan(int fanCommand){
  if ((fanCommand == ON) && (fan == false)){
    Serial.println("");  
    Serial.println("  Start fan");

    fan = true;
  }else if ((fanCommand == OFF) && (fan == true)){
    Serial.println("");  
    Serial.println("  Stop fan");
    fan = false;
  }
}




