# Project Name -> MATE FLOATS (Document : 2026_MATE_Floats_Preview_Mission.pdf)
# Overview
Mission outline for 2026 competition season


## user define parameter
- sp = surface pressure in kPa
- fd = first depth in centimeter
- fdt = first depth monitoring time in second
- sd = second depth in centimeter
- sdt = second depth monitoring time in second

## Hardware
- Control station : A lap-top computer connected to ESP32-S3-DevKit as receiving side
- On-board device : ESP32-S3-DevKit connected to MS5837 pressure sensor with I2C address 0x76
- micro-stepper to control stepper motor through DIR and STEP pin
- 500ml syringe as water displacement engine

## Software
- Build for Control station and On-board ESP32-S3-DevKit PlatformIO in VSCode Arduino Framework
- Control station graph plotter after receiving all the data : teleplot in VSCode

## Program logic flow for both ESP32
- Power on onboard_float.cpp ESP32, sending "Ready" signal to control_station.cpp
- control_station.cpp reveive "Ready" signal, user press predefined button attached to controll_station ESP32 GPIO and then send back "Deploy" signal to onboard_float.cpp
- start recording water pressure data every 4 seconds
- onboard_float.cpp wait 10 seconds, then decend to predefined second depth (sd), wait predefined period of time (sdt), 
- when wait time is over, asscend to predefined first depth (fd), with for predefined period of time (fdt), 
- when wait time is over, then decend to predefined second depth (sd), wait predefined period of time (sdt), 
- when wait time is over, asscend to predefined first depth (fd), with for predefined period of time (fdt), 
- finally asscent back to water surface, stop recording water pressure data, sending "Ready to send" signal to control_station.cpp
-control_station.cpp reveive "Ready to send" signal from onboard_float.cpp, user press predefined button attached to controll_station ESP32 GPIO and then send back "send now" signal to onboard_float.cpp
onboard_float.cpp send whole data packet from mission begin, control_station.cpp receive the whole package and plot it in teleplot

### Physical wiring
- onboard_float 
MS5837 Sensor:
VCC → 3.3V
GND → GND  
SDA → GPIO5 (or GPIO48)
SCL → GPIO6 (or GPIO47)

Stepper Driver:
STEP → GPIO13
DIR → GPIO12
GND → GND
VCC → Appropriate stepper motor power supply

Battery Monitor:
Battery voltage divider middle pin → ADC2_CH0 as in GPIO2 (A2)

- Control station
Buttons:
DEPLOY Button: One side to GPIO15, other side to GND (with pull-up enabled)
SEND NOW Button: One side to GPIO16, other side to GND (with pull-up enabled)