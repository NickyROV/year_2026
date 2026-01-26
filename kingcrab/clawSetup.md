**Claw stack setting**  
Top Pi 400 SSID gripper_top, password : dryside   
ROV Pi 3B SSID wtc, password : wetside

**Network setting**  
*gripper@gripper_top*  
$ls /etc/netplan/  
$sudo nano /etc/netplan/50-cloud-init.yaml  
```yaml
network:  
ethernets:  
  eth0: 
      dhcp4: false  
      addresses:  
        - 192.168.1.10/24  
  version: 2
```
  
$sudo netplan apply

*wtc@ubuntu*    
$ls /etc/netplan/  
$sudo nano /etc/netplan/50-cloud-init.yaml  
```yaml
network:
  ethernets:
    eth0:
      dhcp4: false
      addresses:
        - 192.168.1.11/24
  version: 2
```
  
$sudo netplan apply  

**Network Check**  
$ping 192.168.1.11 (from gripper-top to wtc)  
$ssh wtc@192.168.1.11  
reboot and try again!!  

**ROS2 installation**  
ROS2 Humble on gripper@gripper-top  
*ros-base* on wtc@ubuntu, which is light version without Riz and rqt  
Test with talker and listener (**sudo apt install ros-humble-demo-nodes-cpp ros-humble-demo-nodes-py** as ros-base doesn't preinstall)

**ROS2 build - Control side**  
Create workspace : control_ws/src  
Colcon build -> *~/control_ws$colcon build --symlink-install* to generate *<control_ws>* package  
Colcon test -> *~/control_ws$colcon test*  

Create package : dry top gripper control station
*~/control_ws$cd src*  
*~/control_ws/src$ros2 pkg create control --build-type ament_cmake --dependencies rclcpp*
create cpp in *~/control_ws/src/control/src/keyboard.cpp*  
add executable in CMakelist.txt in *~/control_ws/src/control*  
create node -> *~/control_ws$colcon build*  
source the environment *~/control_ws$source install/setup.bash*
run node -> *~/control_ws$ros2 run control keyboard*  

**Install WiringPi to enable I2C in wtc**  
Install wiringpi


**ROS2 build - WTC side**  
Create workspace : gripper_ws/src  
Colcon build -> *~/gripper_ws$colcon build --symlink-install* to generate *<gripper_ws>* package  
Colcon test -> *~/gripper_ws$colcon test*  

Create package : wet bottom gripper servo wtc
*~/gripper_ws$cd src*  
*~/gripper_ws/src$ros2 pkg create gripper --build-type ament_cmake --dependencies rclcpp*  
create cpp in *~/gripper_ws/src/gripper/src/servo.cpp*
add executable in CMakeList.txt in *~/gripper_ws/src/gripper*
source the environment *~/gripper_ws$source install/setup.bash*
run node -> *~/gripper_ws$ros2 run gripper servo*






**Install PCA9685 on wtc**




