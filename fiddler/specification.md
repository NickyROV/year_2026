# project : SBUS controlling Remote Operated Vehicle (ROV)
# microcontroler : Raspberry Pi Pico  
# Programming interface : Thonny with micro-python


prepare a micro-python program in Thonny environment with Raspberry Pi Pico as hardware to parse SBUS signal (16 channel) form RC controller and interpret (translate) into I2C to PCA9685 to control 10 channel PWM output with following mapping;

channel 1 (Sway) - PWM 0,1,2,3  
channel 2 (Surge) - PWM 0,1,2,3  
channel 3 - (Heave) PWM 4 and 5  
channel 4 (YAW) - PWM 0,1,2,3  
channel 5 - PWM 6  
channel 6  - PWM 7  
channel 7 - PWM 8   
channel 8 - PWM 9  
channel 9 - PWM 10  
channel 10 - PWM 11  
channel 11 - PWM 12  
channel 12 - PWM 13  
channel 13 - PWM 14  
channel 14 - PWM 15  
channel 15 - NOT USE  
channel 16 - NOT USE 

ROV motion using channel 1-4 (PWM 0-3) corresponding to Mecanum wheel configuration + 2 upward mount propeller for take off (both propeller work identically, therefore output PWM 4 and 5 are always equal);
channel 5-14 one to one mapping to PWM 6-15;
channel 11-16 not in use

## Hardward connection
SBUS to pico GP9 (UART1 RX) and PCA9685 SDA & SCL to pico GP0 & GP1 (I2C0)  
4k7 pull up resistor connected to both SDA & SCL correspondingly  
OE pin in PCA9685 is ground connected  
Raspberry Pi t0 3V3, PCS9685 Vcc to 5V and V+ to 5V separately  
All devices are grounded properly  

## GPIO LED as system indicator
GPIO14 -> LED_GOOD, turn ON when I2C connected no problem,  
GPIO15 -> LED_ERROR, turn ON when I2C error
GPIO16 -> LED_HEARTBEAT, toggle when looping


### Program precaution
1. Avoid Bit-shifting misalignment and Sync drifting, parse all 16 channels first and map to appropriate PWM output
2. Bit Packing
3. Buffer Overflow
4. Deadband
