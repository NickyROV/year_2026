cmake_minimum_required(VERSION 3.8)
project(gripper)  # Changed to a more generic project name

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find common ROS2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# ----- Keyboard Node Configuration -----
add_executable(keyboard src/keyboard.cpp)
target_include_directories(keyboard PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(keyboard
  rclcpp
  std_msgs)

# ----- Servo Node Configuration -----
# WiringPi configuration
find_path(WIRINGPI_INCLUDE_DIR wiringPi.h
  PATHS /usr/local/include /usr/include
)
find_library(WIRINGPI_LIB wiringPi
  PATHS /usr/local/lib /usr/lib
)

if(NOT WIRINGPI_INCLUDE_DIR OR NOT WIRINGPI_LIB)
  message(FATAL_ERROR "WiringPi not found. Install with: cd ~/WiringPi && ./build")
endif()

add_executable(servo src/servo.cpp)
target_include_directories(servo PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${WIRINGPI_INCLUDE_DIR})
ament_target_dependencies(servo
  rclcpp
  std_msgs)
target_link_libraries(servo
  ${WIRINGPI_LIB})

# ----- Installation -----
install(TARGETS
  keyboard
  servo
  DESTINATION lib/${PROJECT_NAME})

# ----- Testing -----
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
