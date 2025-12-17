**Fremework** : ROS2 Humble Hawksbill   
Surface Pi: Raspberry pi400 with keyboard running ROS2 Humble on Ubuntu Jammy Desktop   
Surface Router: None  

**Communication Interface** : Ethernet Cat5   
ROV-WTC Pi: Raspberry Pi3 running ROS2 (ros-base) on Ubuntu Server   
ROV-WTC I2C: pca9685 16 servos controller (I2C)  



|Hardware|Surface|ROV_WTC|
|---|---|---|
|Feature|dryside|wetside|
|Workspace|control_ws|execute_ws|
|Package|control|execute|
|Programe|keyboard.cpp|servo.cpp|
|Node|keyboard|servo|
|node in rqt|/keyboard_node|/servo_controller|
|Topic /Keyboard_command|publish|subscribe|

**Setup Procedure**  
**Power source** : USB-C/27W for Pi and separate Buck Converter to maximum 6V for PCA9685   
**Sudo** : update and upgrade as usual  
**Suface Pi with Password "dryside"**  
~/control_ws$source install/setup.bash  
$ros2 run control keyboard  
  
**ROV_WTC pi with password "wetside" via ssh**  
$ssh wtc@192.168.1.11 (static IP)  
~/execute_ws$source install/setup.bash  
$ros2 run execute servo  


