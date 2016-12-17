# Greenhouse-controller
A cheap but efficient controller for small-scale farmers looking to automate their greenhouse.
Allow control on : 
- 2 rollups opening and closing simultaneously
- 1 fan starting when temperature is 2 degrees C above normal
- 1 heater starting when temperature is 2 degrees C below normal
- 1 second heater starting when temperature is 5 degrees C below normal


Material needed : 
- (1) arduino uno
- (1) dht11 temperature & humidity sensor
- (1) i2c 20x4 LCD from DFRobot
- (1) 8 optoisolated relay module from Sainsmart
- jumpers and wires

Version of arduino IDE : 1.6.12

Libraries included :
[Adafruit Unified Sensor Library](https://github.com/adafruit/Adafruit_Sensor)
[DHT Sensor Library](https://github.com/adafruit/DHT-sensor-library)
[NewliquidCrystal v.1.3.4](https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads)
