# Greenhouse-controller (with LCD interface)
A cheap but efficient controller for small-scale farmers looking to automate their greenhouse.

Allow control on : 
- up to 5 time-zones with different temperature settings and activation temperature for each devices
    Parameter for timezones are :
    - Type (Based on sunrise (according to your latitude/longitude/local hour using an astronomical algorithm), manual
      or based on sunset)
    - Time (minuts before or after for sunrise/sunset mode (max. 60), hour and minut for manual mode)
    - Target Temperature
   
- up to 2 rollups motors opening and closing in a predefined number of increments, with pause in between to allow air flow
    Parameters for rollups are :
    - Number of increments
    - Rotation time during each increment
    - Pause time during each increment
    - Activation temperature modificator (according to target temperature)
    - Hysteresis
    - A safety mode : allow a switch to stop the increment counter
    
- up to 2 fans starting when a specific temperature is reached (depending of the time-zone)
    Parameters for fans are :
   - Activation temperature modificator (according to target temperature)
   - Hysteresis
   - A safety mode : allow fan to be started only if a switch is pushed/activated
   
- up to 2 heaters starting when a specific temperature is reached (depending of the time-zone)
   - Activation temperature modificator (according to target temperature)
   - Hysteresis

The controller has two modes(menu and controle) which can't be run simultaneously. Either you program the controler using the LCD menu, either you run the actual control program. 
With the menu, the user can program:
[in construction]

Material needed : 
- arduino uno
- ds18b20 temperature probe
- [DS3231 Real-time-clock module](https://abra-electronics.com/robotics-embedded-electronics/breakout-boards/clocks/ard-ds3231-super-accurate-real-time-clock-ds3231.html)
- [i2c 20x4 LCD from DFRobot](https://www.dfrobot.com/wiki/index.php/I2C_TWI_LCD2004_Module_(Arduino/Gadgeteer_Compatible))
- [8 optoisolated relay module from Sainsmart](http://www.sainsmart.com/8-channel-dc-5v-relay-module-for-arduino-pic-arm-dsp-avr-msp430-ttl-logic.html)
- screw shield
- 3 buttons 
- 2 on/off switch
- jumpers and wires

Version of arduino IDE : 1.6.12

Libraries:
- DS3231
- NewLiquidCrystal
- OneWire
- Dallas_Temperature
- TimeLord
