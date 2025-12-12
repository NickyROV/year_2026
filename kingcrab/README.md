# kingcrab_2026
Two layers independent control  
|Specification|Movement Stack|Claw Stack| 
|-----|-----|-----|
|Use Case|ROV manoeuvre|Claws control|
|Framework|BuleOS+Cockpit|ROS2 Humble|
|Dry(Top) side Hardware/Software|Desktop/Linux|Pi4/Ubuntu Desktop + Humble Desktop|
|Wet(Bottom) side Hardware/Software|Pi4+Navigator/BlueOS|Pi3B/Ubuntu Server + Humble Base|
|Commnication Protocol|MAVLink|TCP/IP|
|Interface|Web-base|CLI|
|DOF|4 axis movement|12 arm movement|
|Actuator|6 Thrusters|12 Servos|
|-----|-----|-----| 

Claw stack setting;  
Top Pi 400 SSID gripper_top, password : dryside  
ROV Pi 3B SSID wtc, password : wetside  
