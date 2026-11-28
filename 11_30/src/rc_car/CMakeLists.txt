cmake_minimum_required(VERSION 3.16)
project(rc_car)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

# PC(WSL)에서 실행하는 노드만 colcon 으로 빌드 (motor_pub)
# src/motor_sub.cpp + motor_sub.ino 는 ESP32용 -> arduino-cli 로 컴파일해서 ESP32에 업로드
add_executable(motor_pub src/motor_pub.cpp)
ament_target_dependencies(motor_pub rclcpp geometry_msgs)

install(TARGETS
  motor_pub
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
