/*
Greenhouse.h -  A library to manage greenhouse items.
Created by : Loup Hébert-Chartrand, April 18, 2017.
Released into the public domain.
*/

#ifndef Greenhouse_h
#define Greenhouse_h

#include "Arduino.h"
#include "EEPROM.h"

//******************EEPROM INDEX*********************
#define TIMEARRAY 3
#define PROGRAMS 15
#define SRMOD 20
#define SSMOD 25
#define TEMPCIBLE 30
#define RMOD 35
#define RHYST 37
#define INCREMENTS 39
#define ROTATION 41
#define PAUSE 43
#define ROLLUPSAFETY 45
#define VMOD 47
#define VHYST 49
#define FANSAFETY 51
#define HMOD 53
#define HHYST 55
#define RAMPING 57

//******************VOCABULARY*************************
#define HEURE 2
#define MINUTE 1
#define SR 1
#define CLOCK 2
#define SS 3
#define CLOSE LOW
#define OPEN HIGH
#define ON HIGH
#define OFF LOW


/*Définition des programmes (#, TYPE, HEURE, MINUTE, TEMPÉRATURE CIBLE)
  TYPE:
  -SR : Heure définie à partir du lever du soleil, laisser la case HEURE vide et inscrire le décalage en minutes dans la case MINUTE
        ex. (1, SR, 0, -30[...]) signifie que le programme 1 débute 30 minutes avant le lever du soleil
  -CLOCK : Heure définie manuellement, inscrire l'heure dans la case HEURE et la minute dans la case MINUTE
  -SS : Heure définie à partir du coucher du soleil, (consignes identiques à SR)

*/
    void defineProgram(byte number, byte type, byte hours, int minuts, byte tempCible);

/*Définition des rollups (#, INCREMENTS, TEMPS DE ROTATION(s), TEMPS DE PAUSE(s), MODIFICATEUR, HYSTERESIS, SÉCURITÉ)
  # : # de l'item (max. 2)
  INCREMENTS : Nombre d'étapes dans l'ouverture/la fermeture
  TEMPS DE ROTATION(s) : temps de rotation (en secondes) pour chaque incrément d'ouverture/fermeture
  TEMPS DE PAUSE(s) : Temps de pause entre chaque séquence d'ouverture
  MODIFICATEUR : température d'activation, calculée à partir de la température cible + le valeur du modificatuer
    ex. Si la température est de 22C et le modificateur est à -2, la température d'activation sera à 20C.
  HYSTERESIS : décalage autorisé par rapport à la température d'activation.
    ex. Si la température d'activation est de 20C, et l'hysteresis de 1, le rollup redescendra seulement si la température descend en bas de 19C
  SÉCURITÉ: Si activée (true), arrête la course du moteur lorsqu'un circuit de sécurité est ramené à la terre
    ex. limitswitch ou switch magnétique, située en fin de course du moteur
*/

    void defineRollup(byte number, byte increments, byte rotation, byte pause, int rmod, byte hysteresis, boolean safety);

/*Définition des sorties de ventilation (#, MODIFICATEUR, HYSTERESIS, SÉCURITÉ)
  # : # de l'item (max. 2)
  MODIFICATEUR : température d'activation, calculée à partir de la température cible + le valeur du modificatuer
    ex. Si la température est de 22C et le modificateur est à 2, la température d'activation sera à 24C.
  HYSTERESIS : décalage autorisé par rapport à la température d'activation.
    ex. Si la température d'activation est de 24C, et l'hysteresis de 1, la fan s'arrêtera quand la température redescendra en bas de 23C
  SÉCURITÉ : Si activée (true), empêche l'activation de la fan à moins qu'un circuit de sécurité soit ramené à la terre
    ex. limitswitch ou switch magnétique, située en fin de course du moteur
*/

    void defineFan(byte number, int vmod, byte hysteresis, boolean security);

/*Définition des sorties de chauffage (#, MODIFICATEUR, HYSTERESIS)
    # : # de l'item (max. 2)
    MODIFICATEUR : température d'activation, calculée à partir de la température cible + le valeur du modificatuer
      ex. Si la température est de 22C et le modificateur est à -5, la température d'activation sera à 17C.
    HYSTERESIS : décalage autorisé par rapport à la température d'activation.
      ex. Si la température d'activation est de 17C, et l'hysteresis de 1, la fournaise s'arrêtera quand la température sera remontée en haut de 18C
*/
    void defineHeater(byte number, int hmod, byte hysteresis);

/*Définition du temps de rampe pour chaque augmentation de 0,5C de la température cible.
  ex. defineRamping(5) signifie que le controleur va attendre 5 minutes avant d'augmenter la température cible de 0,5C
*/
    void defineRamping(byte ramping);


    void initRollupOutput(byte number, byte rollup_open, byte rollup_close, boolean state);
    void initFanOutput(byte number, byte fanPin, boolean state);
    void initHeaterOutput(byte number, byte heaterPin, boolean state);
    byte negativeToByte(int data, byte mod);
    int byteToNegative(int data, byte mod);
    byte PROGRAM_TIME(byte counter, byte timeData);
    void convertDecimalToTime(int * heure, int * minut);
    void lcdPrintDigits(int digits);
    void serialPrintDigits(int digits);


#endif
