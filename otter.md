otter is a 6 thruster ROV base on blue robotic navigator + copilot platform as a backup plan if project fail

___
claw management through gripper setting is only supported by Ardupilot 4.5.3, which means it has to be roll back down as the updated version as of now is 4.5.5, check the following link for a full setup procedure,
https://discuss.bluerobotics.com/t/bluerov2-heavy-gripper-not-working-no-pwm-output-for-gripper/21523/3
___
For some reason, it doesn't work when start again, gripper PWM return to 0ms, so I just map the corresponding 2 claw to Light 1 and Light 2
1. Assign Light 1 (RCIN9) and Light 2 (RCIN10) in BlueOS Vehicle setup
2. Map joystick button accordingly

otter equipped with 6 water-cool ESC alongside 6 thruster, whcih make room for components in WTC 
