
#include "Arduino.h"
#include "Greenhouse.h"
#include "EEPROM.h"

//**************************************************************
//**********       MACROS - INITIALISATION      ****************
//**************************************************************

void defineProgram(byte number, byte type, byte hours, int minuts, byte tempCible){
      byte indexNumber = number-1;
    EEPROM.update(PROGRAMS+indexNumber, type);
    if (type == SR){
      EEPROM.update(SRMOD+indexNumber, negativeToByte(minuts, 60));
    }
    else if(type == CLOCK){
      EEPROM.update((indexNumber)*TIMEARRAY+HEURE, hours);
      EEPROM.update((indexNumber)*TIMEARRAY+MINUTE, minuts);
    }
    else if(type == SS){
      EEPROM.update(SSMOD+indexNumber, negativeToByte(minuts, 60));
    }
    EEPROM.update(TEMPCIBLE+indexNumber, tempCible);
}

void defineRollup(byte number, byte increments, byte rotation, byte pause, int rmod, byte hysteresis, boolean safety){
      byte indexNumber = number-1;
    EEPROM.update(INCREMENTS+indexNumber, increments);
    EEPROM.update(ROTATION+indexNumber, rotation);
    EEPROM.update(PAUSE+indexNumber, pause);
    EEPROM.update(RMOD+indexNumber, negativeToByte(rmod, 10));
    EEPROM.update(RHYST+indexNumber, hysteresis);
    EEPROM.update(ROLLUPSAFETY+indexNumber, safety);

}

void defineFan(byte number, int vmod, byte hysteresis, boolean safety){
    byte indexNumber = number-1;
  EEPROM.update(VMOD+indexNumber, negativeToByte(vmod, 10));
  EEPROM.update(VHYST+indexNumber, hysteresis);
  EEPROM.update(FANSAFETY+indexNumber, safety);

}

void defineHeater(byte number, int hmod, byte hysteresis){
    byte indexNumber = number-1;
  EEPROM.update(HMOD+indexNumber, negativeToByte(hmod, 10));
  EEPROM.update(HHYST+indexNumber, hysteresis);
}

void defineRamping(byte ramping){
  EEPROM.update(RAMPING, ramping);
}

void initRollupOutput(byte number, byte rollup_open, byte rollup_close, boolean state){
    pinMode(rollup_open, OUTPUT);
    digitalWrite(rollup_open, state);
    pinMode(rollup_close, OUTPUT);
    digitalWrite(rollup_close, state);
}

void initFanOutput(byte number, byte fanPin, boolean state){
    pinMode(fanPin, OUTPUT);
    digitalWrite(fanPin, state);
}
void initHeaterOutput(byte number, byte heaterPin, boolean state){
    pinMode(heaterPin, OUTPUT);
    digitalWrite(heaterPin, state);
}
//**************************************************************
//**********       MACROS - AFFICHAGE     ****************
//**************************************************************

int programsE(byte index){
  return EEPROM.read(PROGRAMS+index);
}
int srmodE(byte index){
  return (int)EEPROM.read(SRMOD+index)-60;
}
int ssmodE(byte index){
  return (int)EEPROM.read(SSMOD+index)-60;
}
int targetTempE(byte index){
  return EEPROM.read(TEMPCIBLE+index);
}
unsigned long rampingE(){
  return (unsigned long)EEPROM.read(RAMPING)*60*1000;
}
int rmodE(byte index){
  return (int)EEPROM.read(RMOD+index) -10;
}
int vmodE(byte index){
  return (int)EEPROM.read(VMOD+index) -10;
}
int hmodE(byte index){
  return (int)EEPROM.read(HMOD+index) -10;
}
int rhystE(byte index){
  return (int)EEPROM.read(RHYST+index);
}
int vhystE(byte index){
  return (int)EEPROM.read(VHYST+index);
}
int hhystE(byte index){
  return (int)EEPROM.read(HHYST+index);
}
unsigned int pauseE(byte index){
  return EEPROM.read(PAUSE+index) * 1000;
}
unsigned int rotationE(byte index){
  return EEPROM.read(ROTATION+index) * 1000;
}
int incrementsE(byte index){
  return EEPROM.read(INCREMENTS+index);
}
boolean rollupSafetyE(byte index){
  return EEPROM.read(ROLLUPSAFETY+index);
}
boolean fanSafetyE(byte index){
  return EEPROM.read(FANSAFETY+index);
}
//**************************************************************
//**********       MACROS - OUTILS      ****************
//**************************************************************
byte negativeToByte(int data, byte mod){
  return data+mod;
}

int byteToNegative(int data, byte mod){
  return data-mod;
}
byte PROGRAM_TIME(byte counter, byte timeData){
  return EEPROM.read(TIMEARRAY*counter+timeData);
}
//Programme pour convertir l'addition de nombres décimales en format horaire
void convertDecimalToTime(int * heure, int * minut){
  //Serial.println(m);
  if ((*minut > 59) && (*minut < 120)){
    *heure += 1;
    *minut -= 60;
  }
  else if ((*minut < 0) && (*minut >= -60)){
    *heure -= 1;
    *minut +=60;
  }
}
