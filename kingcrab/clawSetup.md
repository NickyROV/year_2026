Claw stack setting;  
Top Pi 400 SSID gripper_top, password : dryside   
ROV Pi 3B SSID wtc, password : wetside

**Network setting**  
gripper@gripper_top ->
$ls /etc/netplan/  
$sudo nano /etc/netplan/50-cloud-init.yaml  
network:  
  ethernets:  
    eth0:  
      dhcp4: false  
      addresses:  
        - 192.168.1.10/24  
  version: 2  
$sudo netplan apply


wtc@ubuntu ->  
$ls /etc/netplan/  
$sudo nano /etc/netplan/50-cloud-init.yaml  
network:  
  ethernets:  
    eth0:  
      dhcp4: false  
      addresses:  
        - 192.168.1.11/24  
  version: 2  
$sudo netplan apply  

**Network Check**  
$ping 192.168.1.11 (from gripper-top to wtc)  
$ssh wtc@192.168.1.11  
reboot and try again!!  

**ROS2 installation**  
ROS2 Humble on gripper@gripper-top  
*ros-base* on wtc@ubuntu, which is light version without Riz and rqt

**Install PCA9685 on wtc**




